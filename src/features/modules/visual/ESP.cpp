#include "ESP.hpp"
#include <Windows.h>
#include <vector>
#include "../../../sdk/client/ClientInstance.hpp"
#include "../../../sdk/level/Level.hpp"
#include "../../../sdk/entity/Actor.hpp"
#include "../../../utils/GameUtils.hpp"
#include "../../../utils/RenderUtil.hpp"
#include "../../../utils/logger.hpp"
#include "../../GuiTheme.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <chrono>
#include <condition_variable>
#include <mutex>



ESP::ESP() : Feature("ESP", Category::Visual) {
    this->Description = "Draws themed 2D boxes around nearby players/entities";
    this->Enabled     = false;
    this->Keybind     = 0;
    this->Visibility  = false;
    this->CallAllTime = true;
    this->IsBackground = false;

    AddSlider("Range", &Range, 10.0f, 150.0f, "%.0f");
}



static ImU32 ThemeCol(unsigned argb, float alpha) {
    unsigned a = (argb >> 24) & 0xFF;
    unsigned b = (argb >> 16) & 0xFF;
    unsigned g = (argb >> 8) & 0xFF;
    unsigned r = (argb >> 0) & 0xFF;
    return IM_COL32(r & 0xFF, g & 0xFF, b & 0xFF, (unsigned char)(a * alpha));
}

static bool worldToScreen(const Vec3& worldPos, ImVec2& screen, const Vec3& camPos, const Vec2& camRot, float width, float height) {
    constexpr float DEG_TO_RAD = 3.14159265359f / 180.0f;
    constexpr float FOV_DEG = 70.0f;

    
    
    float dx = worldPos.x - camPos.x;
    float dy = worldPos.y - camPos.y;
    float dz = worldPos.z - camPos.z;

    
    
    
    float yawRad = (camRot.y + 90.0f) * DEG_TO_RAD;
    float pitchRad = -camRot.x * DEG_TO_RAD;

    float cosYaw = cosf(yawRad);
    float sinYaw = sinf(yawRad);
    float cosPitch = cosf(pitchRad);
    float sinPitch = sinf(pitchRad);

    float rotX = dx * cosYaw + dz * sinYaw;
    float rotZ = dz * cosYaw - dx * sinYaw;
    float rotY = dy * cosPitch - rotZ * sinPitch;
    rotZ = dy * sinPitch + rotZ * cosPitch;

    if (rotZ <= 0.1f) return false;

    float fovFactor = width / (2.0f * tanf(FOV_DEG * 0.5f * DEG_TO_RAD));
    float mX = width / 2.0f;
    float mY = height / 2.0f;

    screen.x = mX + (rotX * fovFactor / rotZ);
    screen.y = mY - (rotY * fovFactor / rotZ);

    return (screen.x >= 0 && screen.x <= width && screen.y >= 0 && screen.y <= height);
}


static void GetModuleBounds(uintptr_t& lo, uintptr_t& hi) {
    static uintptr_t s_lo = 0, s_hi = 0;
    if (s_lo && s_hi) { lo = s_lo; hi = s_hi; return; }
    char* modBase = (char*)GetModuleHandleW(nullptr);
    if (modBase) {
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)modBase;
        if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
            IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(modBase + dos->e_lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE) {
                s_lo = (uintptr_t)modBase;
                s_hi = (uintptr_t)modBase + nt->OptionalHeader.SizeOfImage;
            }
        }
    }
    lo = s_lo; hi = s_hi;
}







static void CollectActors(std::vector<uintptr_t>& out, void* localPlayer) {
    if (!localPlayer) return;
    __try {
        uintptr_t modLo, modHi;
        GetModuleBounds(modLo, modHi);
        if (!modHi) return;
        uintptr_t lo = modLo, hi = modHi;

        
        uintptr_t anchorLo = UINTPTR_MAX, anchorHi = 0;
        {
            uintptr_t a[4] = {
                (uintptr_t)localPlayer,
                *(uintptr_t*)((char*)localPlayer + 0x218),
                *(uintptr_t*)((char*)localPlayer + 0x220),
                *(uintptr_t*)((char*)localPlayer + 0x228)
            };
            for (int i = 0; i < 4; i++)
                if (a[i]) {
                    if (a[i] < anchorLo) anchorLo = a[i];
                    if (a[i] > anchorHi) anchorHi = a[i];
                }
        }
        if (!anchorHi) return;
        uintptr_t envLo = anchorLo - (uintptr_t)(8ull << 30);
        if (envLo > anchorLo) envLo = 0;
        uintptr_t envHi = anchorHi + (uintptr_t)(8ull << 30);
        if (envHi < anchorHi) envHi = UINTPTR_MAX;

        
        
        
        uintptr_t trustedReg = 0;
        uintptr_t localCtx = *(uintptr_t*)((char*)localPlayer + 0x8);
        if (localCtx)
            __try { trustedReg = *(uintptr_t*)((char*)localCtx + 0x8); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}

        out.clear();
        uintptr_t base = 0;
        for (;;) {
            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQuery((void*)base, &mbi, sizeof(mbi))) break;
            if (!mbi.RegionSize) break;
            uintptr_t regionBase = (uintptr_t)mbi.BaseAddress;
            uintptr_t regionEnd = regionBase + mbi.RegionSize;
            if (regionEnd < base) break;
            base = regionEnd;
            bool interesting = (mbi.State == MEM_COMMIT) &&
                               (mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_WRITECOPY) &&
                               (mbi.Type == MEM_PRIVATE) &&
                               regionEnd > envLo && regionBase < envHi;
            if (!interesting) continue;
            uintptr_t p = (regionBase + 7) & ~(uintptr_t)7;
            for (; p + 0x230 <= regionEnd && out.size() < 512; p += 8) {
                uintptr_t vt = *(uintptr_t*)p;
                if (vt < lo || vt >= hi) continue; 
                uintptr_t sv   = *(uintptr_t*)(p + 0x218);
                uintptr_t arc  = *(uintptr_t*)(p + 0x228);
                uintptr_t aabb = *(uintptr_t*)(p + 0x220);
                uintptr_t ctx  = *(uintptr_t*)(p + 0x8);
                if (!sv || !arc || !aabb || !ctx) continue;
                if (sv < 0x10000 || arc < 0x10000 || aabb < 0x10000) continue;
                __try {
                    
                    
                    uintptr_t regSelf = 0, ereg = 0;
                    __try { regSelf = *(uintptr_t*)ctx; } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    __try { ereg = *(uintptr_t*)((char*)ctx + 0x8); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    if (trustedReg) {
                        if (regSelf != ctx || ereg != trustedReg) continue;
                    }

                    Vec3 pos = *(Vec3*)sv;
                    Vec3 posOld = *(Vec3*)((char*)sv + 12);
                    Vec2 rot = *(Vec2*)arc;
                    float bx0 = *(float*)((char*)aabb + 0);
                    float by0 = *(float*)((char*)aabb + 4);
                    float bz0 = *(float*)((char*)aabb + 8);
                    float bx1 = *(float*)((char*)aabb + 12);
                    float by1 = *(float*)((char*)aabb + 16);
                    float bz1 = *(float*)((char*)aabb + 20);

                    bool okPos = std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z) &&
                                 pos.y > -200.0f && pos.y < 2000.0f &&
                                 pos.x > -1.0e6f && pos.x < 1.0e6f && pos.z > -1.0e6f && pos.z < 1.0e6f;
                    bool okOld = std::isfinite(posOld.x) && std::isfinite(posOld.y) && std::isfinite(posOld.z) &&
                                 fabsf(posOld.x - pos.x) < 5000.0f && fabsf(posOld.y - pos.y) < 2000.0f && fabsf(posOld.z - pos.z) < 5000.0f;
                    bool okRot = std::isfinite(rot.x) && std::isfinite(rot.y) &&
                                 rot.x >= -90.0f && rot.x <= 90.0f && rot.y >= -360.0f && rot.y <= 360.0f;
                    bool okBox = std::isfinite(bx0) && std::isfinite(by0) && std::isfinite(bz0) &&
                                 std::isfinite(bx1) && std::isfinite(by1) && std::isfinite(bz1) &&
                                 bx1 >= bx0 && by1 >= by0 && bz1 >= bz0 &&
                                 (bx1 - bx0) > 0.2f && (bx1 - bx0) < 30.0f &&
                                 (by1 - by0) > 0.2f && (by1 - by0) < 30.0f &&
                                 (bz1 - bz0) > 0.2f && (bz1 - bz0) < 30.0f;
                    if (okPos && okOld && okRot && okBox) {
                        out.push_back(p);
                        static int s_logEvery = 0;
                        if (++s_logEvery >= 6) {
                            s_logEvery = 0;
                            Logger::InfoTag("ESP", "  actor=0x%p pos=(%.1f,%.1f,%.1f) box=(%.1f,%.1f,%.1f)%s",
                                (void*)p, pos.x, pos.y, pos.z,
                                bx1 - bx0, by1 - by0, bz1 - bz0,
                                p == (uintptr_t)localPlayer ? " [LOCAL]" : "");
                        }
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        out.clear();
    }
}




static std::mutex           s_actorMutex;
static std::vector<uintptr_t> s_actors;
static HANDLE               s_actorThread = nullptr;
static std::atomic<bool>    s_actorRunning{false};
static std::condition_variable s_actorCv;

static void ActorScannerLoop() {
    for (;;) {
        if (!s_actorRunning.load()) break;
        void* localPlayer = ClientInstanceManager::getLocalPlayer();
        std::vector<uintptr_t> next;
        if (localPlayer)
            CollectActors(next, localPlayer);
        {
            std::lock_guard<std::mutex> lock(s_actorMutex);
            s_actors.swap(next);
        }
        std::unique_lock<std::mutex> lock(s_actorMutex);
        if (s_actorCv.wait_for(lock, std::chrono::milliseconds(1200),
                               [] { return !s_actorRunning.load(); }))
            break;
    }
}



static DWORD WINAPI ActorScannerEntry(LPVOID) {
    ActorScannerLoop();
    return 0;
}




static void StopScannerThread(DWORD timeoutMs) {
    if (!s_actorThread) return;
    s_actorRunning = false;
    s_actorCv.notify_all();
    WaitForSingleObject(s_actorThread, timeoutMs);
    CloseHandle(s_actorThread);
    s_actorThread = nullptr;
}

void ESP::OnEnabled() {
    if (!s_actorRunning.exchange(true))
        s_actorThread = CreateThread(nullptr, 0, ActorScannerEntry, nullptr, 0, nullptr);
}

void ESP::OnDisabled() {
    StopScannerThread(3000);
    std::lock_guard<std::mutex> lock(s_actorMutex);
    s_actors.clear();
}




void ESP::StopBackgroundScan() {
    StopScannerThread(1500);
}



static void DrawActorBoxes(ImDrawList* dl, const std::vector<uintptr_t>& actors,
                           const Vec3& myPos, const Vec2& myRot,
                           float w, float h, uintptr_t modLo, uintptr_t modHi,
                           float range, uintptr_t selfAddr, ImU32 fill, ImU32 border,
                           int& actorsSeen, int& boxesDrawn) {
    for (size_t i = 0; i < actors.size(); i++) {
        uintptr_t addr = actors[i];
        if (!addr || addr == selfAddr) continue;
        __try {
            uintptr_t vt = *(uintptr_t*)addr;
            if (vt < modLo || vt >= modHi) continue;
            uintptr_t svP   = *(uintptr_t*)(addr + 0x218);
            uintptr_t aabbP = *(uintptr_t*)(addr + 0x220);
            if (!svP || !aabbP || svP < 0x10000 || aabbP < 0x10000) continue;

            Vec3 apos = *(Vec3*)svP;
            AABB aabb;
            aabb.lower  = *(Vec3*)aabbP;
            aabb.higher = *(Vec3*)(aabbP + 12);

            if (!(std::isfinite(aabb.lower.x) && std::isfinite(aabb.lower.y) && std::isfinite(aabb.lower.z) &&
                  std::isfinite(aabb.higher.x) && std::isfinite(aabb.higher.y) && std::isfinite(aabb.higher.z)))
                continue;
            float dx = aabb.higher.x - aabb.lower.x;
            float dy = aabb.higher.y - aabb.lower.y;
            float dz = aabb.higher.z - aabb.lower.z;
            if (dx <= 0.0f || dy <= 0.0f || dz <= 0.0f) continue;
            if (dx > 30.0f || dy > 30.0f || dz > 30.0f) continue;
            if (myPos.distance(apos) > range) continue;

            actorsSeen++;

            const std::array<Vec3, 8>& corners = aabb.getCorners();
            ImVec2 screenPos;
            float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
            bool anyVisible = false;
            for (const auto& corner : corners) {
                if (worldToScreen(corner, screenPos, myPos, myRot, w, h)) {
                    minX = std::min(minX, screenPos.x);
                    minY = std::min(minY, screenPos.y);
                    maxX = std::max(maxX, screenPos.x);
                    maxY = std::max(maxY, screenPos.y);
                    anyVisible = true;
                }
            }
            if (!anyVisible) continue;

            minX = std::max(0.0f, minX);
            minY = std::max(0.0f, minY);
            maxX = std::min(w, maxX);
            maxY = std::min(h, maxY);
            if (maxX - minX < 2.0f || maxY - minY < 2.0f) continue;

            dl->AddRectFilled({ minX, minY }, { maxX, maxY }, fill);
            dl->AddRect({ minX, minY }, { maxX, maxY }, border, 0.0f, 0, 1.5f);
            boxesDrawn++;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}



static void DumpLocal(void* localPlayer) {
    static bool s_done = false;
    if (s_done || !localPlayer) return;
    s_done = true;
    __try {
        uintptr_t svP   = *(uintptr_t*)((char*)localPlayer + 0x218);
        uintptr_t aabbP = *(uintptr_t*)((char*)localPlayer + 0x220);
        uintptr_t arcP  = *(uintptr_t*)((char*)localPlayer + 0x228);
        if (!svP || !aabbP || !arcP) return;
        Vec3 lp = *(Vec3*)svP;
        Vec2 lr = *(Vec2*)arcP;
        Vec3 lo = *(Vec3*)aabbP;
        Vec3 hi = *(Vec3*)(aabbP + 12);
        Vec3 lp2 = *(Vec3*)((char*)svP + 12); 
        Logger::InfoTag("ESP", "LOCALDUMP pos=(%.2f,%.2f,%.2f) posOld=(%.2f,%.2f,%.2f) rot=(%.2f,%.2f) aabbLo=(%.2f,%.2f,%.2f) aabbHi=(%.2f,%.2f,%.2f)",
            lp.x, lp.y, lp.z, lp2.x, lp2.y, lp2.z, lr.x, lr.y, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void ESP::OnEvent() {
    if (!this->Enabled) return;

    Player* localPlayer = reinterpret_cast<Player*>(ClientInstanceManager::getLocalPlayer());
    Level*  currentLevel = Level::GetCurrent();
    if (!localPlayer || !currentLevel) return;

    Vec3* myPosPtr = localPlayer->getPos();
    Vec2* myRotPtr = localPlayer->getRotation();
    if (!myPosPtr || !myRotPtr) return;

    Vec3 myPos = *myPosPtr;
    Vec2 myRot = *myRotPtr;

    
    std::vector<uintptr_t> actors;
    {
        std::lock_guard<std::mutex> lock(s_actorMutex);
        actors = s_actors;
    }

    
    
    DumpLocal((void*)localPlayer);

    static long long s_lastScanLog = 0;
    long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now - s_lastScanLog > 3000 && !actors.empty()) {
        s_lastScanLog = now;
        Logger::InfoTag("ESP", "actor scan: %zu found", actors.size());
    }

    ImGuiIO& io = ImGui::GetIO();
    float w = io.DisplaySize.x;
    float h = io.DisplaySize.y;
    if (w <= 0 || h <= 0) return;

    static long long s_lastDbgLog = 0;

    uintptr_t modLo, modHi;
    GetModuleBounds(modLo, modHi);

    const auto& a = GuiTheme::GetAccent();
    const float gAlpha = GuiTheme::Alpha();
    ImU32 fill   = ThemeCol(a.SURFACE, 0.40f * gAlpha);
    ImU32 border = ThemeCol(a.PRIMARY, gAlpha);

    ImDrawList* dl = RenderUtil::BgDrawList();
    int actorsSeen = 0, boxesDrawn = 0;

    DrawActorBoxes(dl, actors, myPos, myRot, w, h, modLo, modHi, Range,
                   reinterpret_cast<uintptr_t>(localPlayer), fill, border,
                   actorsSeen, boxesDrawn);

    long long nowDbg = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    if (nowDbg - s_lastDbgLog > 1500) {
        s_lastDbgLog = nowDbg;
        Logger::InfoTag("ESP", "local pos=(%.1f,%.1f,%.1f) rot=(%.1f,%.1f) seen=%d drawn=%d cached=%zu range=%.0f",
            myPos.x, myPos.y, myPos.z, myRot.x, myRot.y, actorsSeen, boxesDrawn, actors.size(), Range);
    }
}