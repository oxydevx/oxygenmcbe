#pragma once
#include <Windows.h>
#include "../../Feature.hpp"
#include <atomic>
#include <mutex>
#include <string>






class Radio : public Feature {
public:
    Radio();
    ~Radio() = default;

    void OnEvent()   override;
    void OnRender()  override;
    void OnEnabled() override;
    void OnDisabled() override;

    
    
    void StopMusic();

private:
    void   OpenMedia();
    void   CloseMedia();
    void   ApplyVolume();
    void   StartBrowse();
    static DWORD WINAPI BrowseProc(LPVOID param);
    static std::wstring Utf8ToWide(const std::string& s);

    char   m_path[260] = { 0 };   
    float  m_volume    = 65.0f;   
    bool   m_repeat    = true;

    bool        m_mediaOpen  = false;
    bool        m_openFail   = false;
    bool        m_playing    = false;   
    int         m_appliedVol = -1;
    wchar_t     m_alias[16]  = { 0 };   
    std::string m_loadedPath;
    long long   m_nextPoll   = 0;
    long long   m_reloadPendingAt = 0;   
    std::string m_failedPath;            
    long long   m_failRetryAt = 0;       
    int         m_statusFails = 0;       
    std::recursive_mutex m_mtx;         

    
    bool      m_uiOpen      = false;
    bool      m_seekDrag    = false;
    float     m_seekPrev    = -1.0f;    
    long long m_seekPending = -1;       
    bool      m_stopPending = false;    
    bool      m_playPending = false;    
    bool      m_manualStop  = false;    
    long long m_posMs       = 0;        
    long long m_lenMs       = -1;       

    
    
    static std::mutex        s_browseMtx;
    static char              s_browseResult[260];
    static bool              s_browsePending;
    static std::atomic<bool> s_browserRunning;

    
    float m_anim  = 0.0f;   
    float m_vel   = 0.0f;   
    float m_wCur  = 64.0f;  
    float m_pulse = 0.0f;   
    float m_expand = 0.0f;  
};