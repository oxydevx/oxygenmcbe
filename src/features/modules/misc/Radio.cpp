#define _CRT_SECURE_NO_WARNINGS
#include "Radio.hpp"
#include "../../Category.hpp"
#include "../../GuiTheme.hpp"
#include "../../../utils/logger.hpp"
#include "../../../utils/Resources.hpp"
#include <windows.h>
#include <mmsystem.h>
#include <commdlg.h>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <chrono>
#include <cstring>
#include <cwchar>
#include <cmath>
#include "imgui.h"


std::mutex        Radio::s_browseMtx;
char              Radio::s_browseResult[260] = { 0 };
bool              Radio::s_browsePending = false;
std::atomic<bool> Radio::s_browserRunning{ false };

namespace {

long long NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}


float EaseOutBack(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    const float u  = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}





bool Mc(const wchar_t* cmd, const char* what, bool quiet = false) {
    MCIERROR err = mciSendStringW(cmd, nullptr, 0, nullptr);
    if (err != 0) {
        if (!quiet) {
            static long long s_lastLogMs = 0;
            const long long now = NowMs();
            if (now - s_lastLogMs > 2000) {
                s_lastLogMs = now;
                wchar_t wbuf[256] = { 0 };
                mciGetErrorStringW(err, wbuf, 256);
                char cbuf[384] = { 0 };
                
                WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, cbuf, sizeof(cbuf), nullptr, nullptr);
                Logger::ErrorTag("Radio", "%s failed (MCI 0x%08lX): %s", what, (unsigned long)err, cbuf);
            }
        }
        return false;
    }
    return true;
}


std::string FileStem(const std::string& path) {
    std::string base = path;
    const size_t slash = base.find_last_of("\\/");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    return base;
}

std::string ToLowerAscii(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    return s;
}




std::wstring DeviceTypeFor(const std::string& path) {
    const size_t d = path.find_last_of('.');
    const std::string ext = (d == std::string::npos) ? "" : ToLowerAscii(path.substr(d));
    if (ext == ".wav")             return L" type waveaudio";
    if (ext == ".mid" || ext == ".midi") return L" type sequencer";
    return L" type MPEGVideo";     
}


std::string FmtMs(long long ms) {
    if (ms < 0) return "--:--";
    const long long s = ms / 1000;
    char b[32];
    snprintf(b, sizeof(b), "%lld:%02lld", s / 60, s % 60);
    return b;
}

} 

std::wstring Radio::Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n : 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], (int)w.size());
    if (n > 0 && w[n - 1] == L'\0') w.resize(n - 1);
    return w;
}

Radio::Radio() : Feature("Radio", Category::Misc) {
    this->Description = "Plays a music track with an animated Dynamic Island";
    this->Enabled      = false;
    this->CallAllTime  = true;   
    this->AddButton("Choose track", [this]() { StartBrowse(); });
    this->AddText("Track path", m_path, 259);
    this->AddSlider("Volume", &m_volume, 0.0f, 100.0f, "%.0f%%");
    this->AddCheckbox("Repeat", &m_repeat);
}

void Radio::OnEnabled() {
    m_pulse = 1.0f;          
}

void Radio::OnDisabled() {
    CloseMedia();            
}

void Radio::StopMusic() {
    CloseMedia();
}

void Radio::OpenMedia() {
    std::lock_guard<std::recursive_mutex> lk(m_mtx);
    
    
    if (m_mediaOpen) Mc((std::wstring(L"close ") + m_alias).c_str(), "close last", true);
    m_mediaOpen  = false;
    m_playing    = false;
    m_appliedVol = -1;
    m_openFail   = false;

    const std::string p = m_path;
    if (p.empty()) { m_loadedPath.clear(); return; }

    
    
    static std::atomic<unsigned int> g_aliasN{ 0 };
    const std::wstring alias = L"ox" + std::to_wstring(g_aliasN.fetch_add(1));
    wcscpy_s(m_alias, alias.c_str());       

    const std::wstring wpath = Utf8ToWide(p);
    const std::wstring cmd = L"open \"" + wpath + L"\"" + DeviceTypeFor(p) + L" alias " + m_alias;
    if (!Mc(cmd.c_str(), "open")) {
        m_loadedPath.clear();    
        m_openFail = true;
        m_failedPath = p;        
        m_failRetryAt = NowMs() + 10000;
        return;
    }
    m_mediaOpen  = true;
    m_loadedPath = p;
    m_failedPath.clear();
    m_failRetryAt = 0;
    m_appliedVol = -1;
    m_nextPoll   = NowMs();
    m_posMs      = 0;
    m_lenMs      = -1;
    m_seekPending = -1;
    m_seekPrev    = -1.0f;
    m_seekDrag    = false;
    m_stopPending = false;
    m_playPending = false;
    m_manualStop  = false;

    ApplyVolume();
    Mc((L"play " + std::wstring(m_alias) + L" from 0").c_str(), "play");
    m_playing = true;
    m_pulse = 1.0f;          
}

void Radio::CloseMedia() {
    std::lock_guard<std::recursive_mutex> lk(m_mtx);
    if (m_mediaOpen) {
        Mc((L"stop " + std::wstring(m_alias)).c_str(), "stop", true);
        Mc((L"close " + std::wstring(m_alias)).c_str(), "close", true);
    }
    m_mediaOpen       = false;
    m_playing         = false;
    m_appliedVol      = -1;
    m_reloadPendingAt = 0;
    m_loadedPath.clear();
    m_failedPath.clear();
    m_failRetryAt = 0;
    m_statusFails = 0;
    m_openFail = false;
    m_posMs      = 0;
    m_lenMs      = -1;
    m_seekPending = -1;
    m_seekPrev    = -1.0f;
    m_seekDrag    = false;
    m_stopPending = false;
    m_playPending = false;
    m_manualStop  = false;
}

void Radio::ApplyVolume() {
    if (!m_mediaOpen) return;
    float vol = m_volume;                              
    if (!(vol >= 0.0f && vol <= 100.0f)) vol = 0.0f;
    const int v = (int)vol * 10;                       
    if (v == m_appliedVol) return;
    const std::wstring c = L"setaudio " + std::wstring(m_alias) + L" volume to " + std::to_wstring(v);
    Mc(c.c_str(), "setaudio volume");
    m_appliedVol = v;
}

void Radio::StartBrowse() {
    if (s_browserRunning.exchange(true)) return;   
    HANDLE h = CreateThread(nullptr, 0, BrowseProc, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
    else   s_browserRunning = false;
}

DWORD WINAPI Radio::BrowseProc(LPVOID) {
    char buf[MAX_PATH] = { 0 };
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Audio (*.mp3;*.wav;*.mid;*.midi;*.aac)\0*.mp3;*.wav;*.mid;*.midi;*.aac\0All files\0*.*\0";
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle  = "Choose a track for Radio";
    if (GetOpenFileNameA(&ofn)) {
        
        
        char u8[MAX_PATH] = { 0 };
        {
            const int wn = MultiByteToWideChar(CP_ACP, 0, buf, -1, nullptr, 0);
            wchar_t wbuf[MAX_PATH] = { 0 };
            if (wn > 0 && wn < MAX_PATH)
                MultiByteToWideChar(CP_ACP, 0, buf, -1, wbuf, wn);
            WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, u8, (int)sizeof(u8), nullptr, nullptr);
        }
        std::lock_guard<std::mutex> lk(s_browseMtx);
        std::strncpy(s_browseResult, u8, sizeof(s_browseResult) - 1);
        s_browsePending = true;
    }
    s_browserRunning = false;
    return 0;
}

void Radio::OnEvent() {
    
    if (s_browsePending) {
        std::lock_guard<std::mutex> lk(s_browseMtx);
        if (s_browsePending) {
            std::strncpy(m_path, s_browseResult, sizeof(m_path) - 1);
            m_path[sizeof(m_path) - 1] = '\0';
            s_browsePending = false;
        }
    }

    if (!Enabled) return;

    std::lock_guard<std::recursive_mutex> lk(m_mtx);

    
    
    if (m_loadedPath != m_path) {
        if (m_reloadPendingAt == 0)
            m_reloadPendingAt = NowMs();
        else if (NowMs() - m_reloadPendingAt >= 350) {
            m_reloadPendingAt = 0;
            
            
            if (m_failedPath == m_path && NowMs() < m_failRetryAt)
                return;
            OpenMedia();
        }
    } else {
        m_reloadPendingAt = 0;
    }
    if (!m_mediaOpen)
        return;

    ApplyVolume();

    
    if (m_seekPending >= 0) {
        if (!m_mediaOpen) OpenMedia();   
        if (m_mediaOpen) {
            Mc((L"seek " + std::wstring(m_alias) + L" to " + std::to_wstring(m_seekPending)).c_str(), "seek");
            Mc((L"play " + std::wstring(m_alias)).c_str(), "play");
            m_posMs = m_seekPending;
            m_manualStop = false;
            m_playing = true;
        }
        m_seekPending = -1;
    }
    if (m_stopPending) {
        Mc((L"stop " + std::wstring(m_alias)).c_str(), "stop");
        m_stopPending = false;
        m_manualStop  = true;
        m_playing     = false;
    }
    if (m_playPending) {
        if (m_mediaOpen) {
            Mc((L"play " + std::wstring(m_alias)).c_str(), "play");
        } else {
            
            
            OpenMedia();
        }
        m_playPending = false;
        m_playing     = m_mediaOpen;
        m_manualStop  = false;
    }

    
    const long long now = NowMs();
    const long long interval = m_uiOpen && m_lenMs > 0 ? 250 : 700;
    if (now >= m_nextPoll) {
        m_nextPoll = now + interval;
        const std::wstring alias = std::wstring(m_alias);

        wchar_t mode[64] = { 0 };
        if (mciSendStringW((L"status " + alias + L" mode").c_str(), mode, 63, nullptr) != 0) {
            
            
            if (++m_statusFails < 3) {
                m_nextPoll = now + interval;
                return;
            }
            m_statusFails = 0;
            
            
            
            
            
            m_mediaOpen = false;
            Mc((L"close " + alias).c_str(), "close stale", true);
            
            if (m_repeat && !m_manualStop)
                OpenMedia();
            return;
        }
        m_statusFails = 0;
        bool playingNow = (wcscmp(mode, L"stopped") != 0);
        if (!playingNow && m_repeat && !m_manualStop) {
            Mc((L"play " + alias + L" from 0").c_str(), "play");
            playingNow = true;
        }
        m_playing = playingNow;

        wchar_t msBuf[32] = { 0 };
        if (mciSendStringW((L"status " + alias + L" position").c_str(), msBuf, 31, nullptr) == 0)
            m_posMs = _wtoi64(msBuf);
        if (m_lenMs < 0) {
            wchar_t lsBuf[32] = { 0 };
            if (mciSendStringW((L"status " + alias + L" length").c_str(), lsBuf, 31, nullptr) == 0) {
                const long long n = _wtoi64(lsBuf);
                if (n > 0) m_lenMs = n;
            }
        }
    }
}

void Radio::OnRender() {
    auto& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) return;
    const float dt = std::min(io.DeltaTime, 0.1f);
    const auto& a  = GuiTheme::GetAccent();
    const float gAlpha = GuiTheme::Alpha();

    
    std::string title;
    if (Enabled && !m_loadedPath.empty())
        title = m_openFail ? "Can't open track" : FileStem(m_loadedPath);
    else if (Enabled)
        title = "Select a track";
    else
        title = "Radio";

    
    ImFont* font = ImGui::GetFont();
    for (size_t i = 0; i < Resources::FontNames.size(); i++) {
        if (Resources::FontNames[i] == "GoogleSans-Medium") { font = Resources::Fonts[i]; break; }
    }
    const float base   = ImGui::GetStyle().FontSizeBase;
    const float txSize = base * 1.15f;

    
    const bool show = Enabled || m_openFail;
    const float tgt = show ? 1.0f : 0.0f;
    m_anim += (tgt - m_anim) * std::min(dt * 8.0f, 1.0f);
    if (std::fabs(m_anim - tgt) < 0.0015f) m_anim = tgt;
    if (m_anim <= 0.002f) return;

    const float al = std::min(m_anim, 1.0f) * gAlpha;
    if (al <= 0.01f) return;

    
    const float H      = 40.0f;                      
    const float padX   = 11.0f;                      
    const int   bars   = 3;
    const float bW     = 3.0f;
    const float bGap   = 4.0f;
    const float eqArea = bars * (bW + bGap) - bGap;  
    const float titleGap = 10.0f;
    const float longTitleW = 190.0f;                 

    const bool  row2 = m_uiOpen && m_mediaOpen;
    const float row2HFull = 30.0f;               

    
    const float expandTgt = row2 ? 1.0f : 0.0f;
    m_expand += (expandTgt - m_expand) * std::min(dt * 10.0f, 1.0f);
    if (std::fabs(m_expand - expandTgt) < 0.0015f) m_expand = expandTgt;
    const float row2H2 = row2HFull * m_expand;   
    const float H_u    = H + row2H2;             

    const float drawSz = txSize * (1.0f + 0.06f * m_pulse);
    const float dW = font->CalcTextSizeA(drawSz, FLT_MAX, 0.0f, title.c_str()).x;
    const bool  overflow = dW > longTitleW;
    const float winInnerW = overflow ? longTitleW : dW;

    
    
    
    const float bs      = base * 0.85f;
    float       row2Win = 0.0f;
    if (row2) {
        const float we = font->CalcTextSizeA(bs, FLT_MAX, 0.0f, FmtMs(m_posMs).c_str()).x;
        const float wd = font->CalcTextSizeA(bs, FLT_MAX, 0.0f, FmtMs(m_lenMs).c_str()).x;
        row2Win = padX + we + 8.0f + 90.0f + 8.0f + wd + 6.0f + 16.0f + padX + 24.0f;
    }

    const float wT = std::max(padX + eqArea + titleGap + winInnerW + padX + 4.0f, row2Win);

    
    const float springK = 130.0f, damp = 9.0f;
    m_vel += (wT - m_wCur) * springK * dt;
    m_vel *= std::max(0.0f, 1.0f - damp * dt);
    m_wCur += m_vel * dt;
    m_wCur = std::clamp(m_wCur, 40.0f, std::min(420.0f, io.DisplaySize.x - 24.0f));

    m_pulse = std::max(0.0f, m_pulse - dt * 3.2f);

    
    const float slide = EaseOutBack(m_anim);
    const float top   = 10.0f;
    const float yOff  = -(H_u + 8.0f) * (1.0f - slide);
    const float cx    = io.DisplaySize.x * 0.5f;
    const float cy    = top + H_u * 0.5f + yOff;

    const float hw = m_wCur * 0.5f;
    const ImVec2 pmin(cx - hw, cy - H_u * 0.5f);
    const ImVec2 pmax(cx + hw, cy + H_u * 0.5f);
    const float radius = std::min(H_u * 0.5f, row2 ? 26.0f : 20.0f);
    const float cy1 = pmin.y + H * 0.5f;   

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    
    for (int i = 3; i >= 1; --i) {
        const float off = (float)i * 2.2f;
        dl->AddRectFilled({ pmin.x, pmin.y + off }, { pmax.x, pmax.y + off },
            IM_COL32(0, 0, 0, (int)((22.0f - (float)i * 6.0f) * al)), radius);
    }

    
    dl->AddRectFilled(pmin, pmax, IM_COL32(12, 12, 15, (int)(245.0f * al)), radius);
    dl->AddRect(pmin, pmax, IM_COL32(255, 255, 255, (int)(16.0f * al)), radius, 0, 1.0f);
    
    dl->AddRectFilled({ pmin.x + 6.0f, pmin.y + 1.0f }, { pmax.x - 6.0f, pmin.y + 2.0f },
        IM_COL32(255, 255, 255, (int)(10.0f * al)), 1.0f);

    const ImVec4 cAcc = ImGui::ColorConvertU32ToFloat4(a.PRIMARY);
    const bool playing = Enabled && m_playing;

    
    const float eqX0  = pmin.x + padX;
    const float eqTop = pmin.y + 12.0f;
    const float eqBot = pmin.y + H - 12.0f;
    const float eqH   = eqBot - eqTop;
    const float tm    = (float)ImGui::GetTime();
    const ImU32 eqCol = ImGui::ColorConvertFloat4ToU32(ImVec4(cAcc.x, cAcc.y, cAcc.z, cAcc.w * al));
    for (int i = 0; i < bars; i++) {
        float h;
        if (playing)
            h = eqH * (0.35f + 0.65f * (0.5f + 0.5f * (float)std::sin(tm * 6.0f + i * 1.9f)));
        else
            h = eqH * 0.18f;   
        const float bx = eqX0 + i * (bW + bGap);
        dl->AddRectFilled({ bx, eqBot - h }, { bx + bW, eqBot },
            playing ? eqCol : IM_COL32(255, 255, 255, (int)(60.0f * al)), 1.0f);
    }

    
    const float titleLeft  = eqX0 + eqArea + titleGap;
    const float innerRight = pmax.x - padX;
    const float viewW      = std::max(1.0f, innerRight - titleLeft);
    const ImU32 titleCol   = IM_COL32(246, 246, 248, (int)(235.0f * al));

    dl->PushClipRect(ImVec2(titleLeft - 2.0f, pmin.y),
                     ImVec2(innerRight + 2.0f, pmax.y), true);
    float tx;
    if (overflow) {
        const float range = std::max(24.0f, dW - viewW);   
        const float speed = 55.0f;
        const float trip  = range / speed;         
        const float cyc   = trip * 2.0f;           
        const float ph    = (float)std::fmod((double)ImGui::GetTime(), (double)cyc);
        const float tri   = (ph < cyc * 0.5)
            ? (float)(ph / (cyc * 0.5))
            : (float)(1.0 - (ph - cyc * 0.5) / (cyc * 0.5));
        tx = titleLeft - tri * range;
    } else {
        tx = innerRight - dW;                      
    }
    dl->AddText(font, drawSz, { tx, cy1 - drawSz * 0.5f - 1.0f }, titleCol, title.c_str());

    
    
    const float fadeW  = 16.0f;
    const float tyMin  = cy1 - drawSz * 0.5f - 4.0f;
    const float tyMax  = cy1 + drawSz * 0.5f + 4.0f;
    const ImU32 fBg    = IM_COL32(12, 12, 15, (int)(245.0f * al));
    const ImU32 fTrans = IM_COL32(12, 12, 15, 0);
    dl->AddRectFilledMultiColor({ titleLeft, tyMin }, { titleLeft + fadeW, tyMax },
        fBg, fTrans, fTrans, fBg);
    dl->AddRectFilledMultiColor({ innerRight - fadeW, tyMin }, { innerRight, tyMax },
        fTrans, fBg, fBg, fTrans);
    dl->PopClipRect();

    
    float barL = 0.0f, barR = 0.0f, barWp = 1.0f, cy2 = 0.0f, stopL = 0.0f, stopW = 16.0f;
    if (m_expand > 0.02f) {
        const float ra = std::clamp(m_expand, 0.0f, 1.0f);   
        const std::string sE = FmtMs(m_posMs);
        const std::string sD = FmtMs(m_lenMs);
        const float we = font->CalcTextSizeA(bs, FLT_MAX, 0.0f, sE.c_str()).x;
        const float wd = font->CalcTextSizeA(bs, FLT_MAX, 0.0f, sD.c_str()).x;

        cy2 = pmin.y + H + row2H2 * 0.5f;
        const float rPad   = std::max(padX, 18.0f);   
        const float innerL = pmin.x + rPad;
        const float innerR = pmax.x - rPad;
        const float stopGap = 6.0f;
        stopL = innerR - stopW;
        const float durL = stopL - stopGap - wd;
        barL = innerL + we + 8.0f;
        barR = durL - 8.0f;
        barWp = std::max(1.0f, barR - barL);

        const ImU32 tCol   = IM_COL32(235, 235, 240, (int)(235.0f * al * ra));
        const ImU32 accCol = ImGui::ColorConvertFloat4ToU32(ImVec4(cAcc.x, cAcc.y, cAcc.z, cAcc.w * al * ra));
        const ImU32 dimAcc = ImGui::ColorConvertFloat4ToU32(ImVec4(cAcc.x, cAcc.y, cAcc.z, cAcc.w * 0.35f * al * ra));

        dl->AddText(font, bs, { innerL, cy2 - bs * 0.5f }, tCol, sE.c_str());
        dl->AddText(font, bs, { durL, cy2 - bs * 0.5f }, tCol, sD.c_str());

        
        const ImVec2 sA(stopL, cy2 - stopW * 0.5f), sB(stopL + stopW, cy2 + stopW * 0.5f);
        dl->AddRectFilled(sA, sB, m_playing ? accCol : dimAcc, 3.5f);
        const ImVec2 cS(stopL + stopW * 0.5f, cy2);
        const ImU32 fg = IM_COL32(12, 12, 15, (int)(245.0f * al * ra));
        if (m_playing) {
            const float gs = 4.0f;
            dl->AddRectFilled({ cS.x - gs * 0.5f, cS.y - gs * 0.5f },
                              { cS.x + gs * 0.5f, cS.y + gs * 0.5f }, fg, 1.0f);
        } else {
            dl->AddTriangleFilled({ cS.x - 2.5f, cS.y - 4.0f },
                                  { cS.x - 2.5f, cS.y + 4.0f },
                                  { cS.x + 3.5f, cS.y }, fg);
        }

        
        const float bh = 3.0f;
        const ImVec2 tA(barL, cy2 - bh * 0.5f), tB(barR, cy2 + bh * 0.5f);
        dl->AddRectFilled(tA, tB, IM_COL32(255, 255, 255, (int)(40.0f * al * ra)), 2.0f);
        float frac = 0.0f;
        if (m_lenMs > 0) frac = std::clamp((float)m_posMs / (float)m_lenMs, 0.0f, 1.0f);
        if (m_seekDrag && m_seekPrev >= 0.0f) frac = std::clamp(m_seekPrev, 0.0f, 1.0f);
        dl->AddRectFilled(tA, { barL + barWp * frac, tB.y }, accCol, 2.0f);
        if (m_lenMs > 0) {
            const float kx = barL + barWp * frac;
            dl->AddCircleFilled({ kx, cy2 }, 4.5f, IM_COL32(255, 255, 255, (int)(255.0f * al * ra)));
            dl->AddCircleFilled({ kx, cy2 }, 2.0f, accCol);
        }
    }

    
    const ImVec2 mp = io.MousePos;
    const bool inPill = mp.x >= pmin.x && mp.x <= pmax.x && mp.y >= pmin.y && mp.y <= pmax.y;
    bool handled = false;
    if (row2 && m_expand > 0.5f) {
        const bool inStop = mp.x >= stopL && mp.x <= stopL + stopW && std::fabs(mp.y - cy2) <= stopW * 0.6f;
        const bool inBar = m_lenMs > 0 && mp.x >= barL && mp.x <= barR && std::fabs(mp.y - cy2) <= 8.0f;
        if (io.MouseClicked[0] && inStop) {
            if (m_playing) m_stopPending = true;
            else           m_playPending = true;
            handled = true;
        }
        else if (io.MouseClicked[0] && inBar) {
            m_seekDrag = true;
            m_seekPrev = std::clamp((mp.x - barL) / barWp, 0.0f, 1.0f);
            handled = true;
        }
        if (m_seekDrag) {
            if (io.MouseDown[0]) {
                m_seekPrev = std::clamp((mp.x - barL) / barWp, 0.0f, 1.0f);
            } else {
                if (m_seekPrev >= 0.0f && m_lenMs > 0)
                    m_seekPending = (long long)(std::clamp(m_seekPrev, 0.0f, 1.0f) * (float)m_lenMs);
                m_seekDrag = false;
                m_seekPrev = -1.0f;
            }
            handled = true;
        }
    }
    if (!handled && io.MouseClicked[0] && inPill)
        m_uiOpen = !m_uiOpen;
}