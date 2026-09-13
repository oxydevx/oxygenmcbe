#include "dxhook.hpp"
#include "../features/ModuleManager.hpp"
#include "../features/modules/visual/ClickGui.hpp"
#include "../utils/Resources.hpp"
#include "../utils/logger.hpp"
#include "../sdk/client/ClientInstance.hpp"
#include "Keyboard.hpp"
#include <safetyhook.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <MinHook.h>
#include "background_blur.hpp"



extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Hooks::DX12 {

    using Present_t = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
    using ExecCmdLists_t = void(WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

    Present_t o_Present = nullptr;
    ExecCmdLists_t o_ExecCmd = nullptr;
    ID3D12CommandQueue* g_cmdQueue = nullptr;
    bool g_imguiInit = false;
    static HWND g_hwnd = nullptr;
    static WNDPROC g_oWndProc = nullptr;
    static bool g_wndHooked = false;

    
    
    
    
    
    std::atomic<bool> g_shuttingDown{false};
    std::atomic<long> g_activeRenders{0};

    
    
    
    static void** g_scVtable = nullptr;
    static void** g_cqVtable = nullptr;

    
    
    
    
    static LRESULT CALLBACK OxygenWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        
        
        if (g_shuttingDown.load(std::memory_order_acquire))
            return CallWindowProc(g_oWndProc, h, m, w, l);

        
        
        
        
        if ((m == WM_KEYDOWN || m == WM_KEYUP || m == WM_SYSKEYDOWN || m == WM_SYSKEYUP) &&
            w == VK_TAB &&
            (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
            return 0;
        }

        
        
        
        bool fedImGui = false;
        switch (m) {
            case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
            case WM_XBUTTONDOWN: case WM_XBUTTONUP:
            case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
            case WM_MOUSEMOVE:
            case WM_KEYDOWN: case WM_KEYUP:
            case WM_SYSKEYDOWN: case WM_SYSKEYUP:
            case WM_CHAR: case WM_SYSCHAR: case WM_DEADCHAR: case WM_SYSDEADCHAR:
                ImGui_ImplWin32_WndProcHandler(h, m, w, l);
                fedImGui = true;
                break;
        }
        if (fedImGui && ClickGui::IsGUIActive())
            return 0;                       
        return CallWindowProc(g_oWndProc, h, m, w, l);
    }

    
    
    
    
    typedef BOOL(WINAPI* tSetCursorPos)(int, int);
    typedef BOOL(WINAPI* tClipCursor)(const RECT*);
    static tSetCursorPos oSetCursorPos = nullptr;
    static tClipCursor   oClipCursor   = nullptr;
    static bool          g_cursorHooks = false;

    static BOOL WINAPI hkSetCursorPos(int X, int Y) {
        if (ClickGui::IsGUIActive()) return TRUE; 
        return oSetCursorPos(X, Y);
    }
    static BOOL WINAPI hkClipCursor(const RECT* lpRect) {
        if (ClickGui::IsGUIActive()) return TRUE; 
        return oClipCursor(lpRect);
    }

    static void InstallCursorHooks() {
        if (g_cursorHooks) return;
        MH_STATUS init = MH_Initialize();
        if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) return;
        MH_STATUS s;
        if ((s = MH_CreateHook(&SetCursorPos, &hkSetCursorPos, (void**)&oSetCursorPos)) == MH_OK) {
            MH_EnableHook(&SetCursorPos);
            Logger::InfoTag("HookedPresent", "cursor hook: SetCursorPos");
        } else {
            Logger::WarnTag("HookedPresent", "SetCursorPos hook failed (%d)", (int)s);
        }
        if ((s = MH_CreateHook(&ClipCursor, &hkClipCursor, (void**)&oClipCursor)) == MH_OK) {
            MH_EnableHook(&ClipCursor);
            Logger::InfoTag("HookedPresent", "cursor hook: ClipCursor");
        } else {
            Logger::WarnTag("HookedPresent", "ClipCursor hook failed (%d)", (int)s);
        }
        g_cursorHooks = true;
    }

    static const UINT FRAME_COUNT = 4;
    static ID3D12Device* g_pDevice = nullptr;
    static ID3D12DescriptorHeap* g_srvHeap = nullptr;
    static ID3D12DescriptorHeap* g_rtvHeap = nullptr;
    static ID3D12CommandAllocator* g_cmdAlloc[FRAME_COUNT] = {};
    static ID3D12GraphicsCommandList* g_cmdList = nullptr;
    static D3D12_CPU_DESCRIPTOR_HANDLE g_rtvHandle[1] = {};
    static ID3D12Fence* g_fence = nullptr;
    static UINT64 g_fenceValue = 0;
    static bool g_initInProgress = false;
    static UINT g_lastSwapChainWidth = 0;
    static UINT g_lastSwapChainHeight = 0;
    static float g_resizeStabilizeTimer = 0.0f;
    static const float RESIZE_STABILIZE_TIME = 0.5f;  

    static ClickGui* g_clickGui = nullptr; 

    extern HMODULE g_hModule;

    

    static void* HookVTable(void** vtable, int index, void* newFunc) {
        void* original = vtable[index];
        DWORD old;
        if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_READWRITE, &old))
            return original;
        vtable[index] = newFunc;
        VirtualProtect(&vtable[index], sizeof(void*), old, &old);
        return original;
    }

    static DXGI_FORMAT GetNonSRVFormat(DXGI_FORMAT fmt) {
        return fmt == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM : fmt;
    }

    

    [[nodiscard]] static bool WaitForGpu() {
        if (!g_fence || !g_cmdQueue) {
            Logger::WarnTag("WaitForGpu", "fence or queue is null");
            return false;
        }
        UINT64 val = g_fenceValue;
        if (g_fence->GetCompletedValue() < val) {
            HANDLE evt = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!evt) {
                Logger::ErrorTag("WaitForGpu", "CreateEvent failed");
                return false;
            }
            g_fence->SetEventOnCompletion(val, evt);
            WaitForSingleObject(evt, INFINITE);
            CloseHandle(evt);
        }
        return true;
    }

    [[nodiscard]] static bool SignalFence() {
        if (!g_fence || !g_cmdQueue) {
            Logger::WarnTag("SignalFence", "fence or queue is null");
            return false;
        }
        g_fenceValue++;
        HRESULT hr = g_cmdQueue->Signal(g_fence, g_fenceValue);
        if (FAILED(hr)) {
            Logger::ErrorTag("SignalFence", "Signal failed (0x%08lX)", (unsigned long)hr);
            return false;
        }
        return true;
    }

    

    static void DestroyRenderResources() {
        Logger::InfoTag("DestroyRenderResources", "releasing D3D12 resources");
        (void)WaitForGpu();
        if (g_cmdList) {
            Logger::InfoTag("DestroyRenderResources", "releasing cmd list (0x%p)", (void*)g_cmdList);
            g_cmdList->Release(); g_cmdList = nullptr;
        }
        for (UINT i = 0; i < FRAME_COUNT; i++) {
            if (g_cmdAlloc[i]) {
                Logger::InfoTag("DestroyRenderResources", "releasing alloc[%u] (0x%p)", i, (void*)g_cmdAlloc[i]);
                g_cmdAlloc[i]->Release(); g_cmdAlloc[i] = nullptr;
            }
        }
        if (g_rtvHeap) {
            Logger::InfoTag("DestroyRenderResources", "releasing RTV heap (0x%p)", (void*)g_rtvHeap);
            g_rtvHeap->Release(); g_rtvHeap = nullptr;
        }
        if (g_srvHeap) {
            Logger::InfoTag("DestroyRenderResources", "releasing SRV heap (0x%p)", (void*)g_srvHeap);
            g_srvHeap->Release(); g_srvHeap = nullptr;
        }
        if (g_fence) {
            Logger::InfoTag("DestroyRenderResources", "releasing fence (0x%p)", (void*)g_fence);
            g_fence->Release(); g_fence = nullptr;
        }
        if (g_pDevice) {
            g_pDevice->Release(); g_pDevice = nullptr;
        }
        g_fenceValue = 0;
        Logger::InfoTag("DestroyRenderResources", "done");
    }

    static bool WindowIsHidden() {
        
        if (!g_imguiInit || !g_hwnd) return true;
        
        if (IsIconic(g_hwnd)) return true;

        RECT rc{};
        if (GetClientRect(g_hwnd, &rc)) {
            const LONG w = rc.right - rc.left;
            const LONG h = rc.bottom - rc.top;
            if (w <= 0 || h <= 0) return true;
        }

        return false;
    }

    static void UpdateDisplaySize(IDXGISwapChain* pSwapChain) {
        if (!pSwapChain) {
            ImGui::GetIO().DisplaySize = ImVec2(0.0f, 0.0f);
            return;
        }

        DXGI_SWAP_CHAIN_DESC desc{};
        HRESULT hr = pSwapChain->GetDesc(&desc);
        if (FAILED(hr)) {
            Logger::WarnTag("UpdateDisplaySize", "GetDesc failed (0x%08lX)", (unsigned long)hr);
            ImGui::GetIO().DisplaySize = ImVec2(0.0f, 0.0f);
            return;
        }

        
        if (desc.BufferDesc.Width > 0 && desc.BufferDesc.Height > 0 && 
            desc.BufferDesc.Width <= 16384 && desc.BufferDesc.Height <= 16384) {
            ImGui::GetIO().DisplaySize = ImVec2((float)desc.BufferDesc.Width, (float)desc.BufferDesc.Height);
            return;
        }

        Logger::WarnTag("UpdateDisplaySize", "invalid buffer dimensions (%ux%u)", desc.BufferDesc.Width, desc.BufferDesc.Height);
        ImGui::GetIO().DisplaySize = ImVec2(0.0f, 0.0f);
    }

    static bool CreateRenderResources(IDXGISwapChain3* pSC3, ID3D12Device* pDevice) {
        Logger::InfoTag("CreateRenderResources", "allocating D3D12 resources");

        
        DXGI_SWAP_CHAIN_DESC scDesc{};
        if (FAILED(pSC3->GetDesc(&scDesc))) {
            Logger::ErrorTag("CreateRenderResources", "failed to get swapchain desc");
            return false;
        }

        if (scDesc.BufferDesc.Width == 0 || scDesc.BufferDesc.Height == 0) {
            Logger::ErrorTag("CreateRenderResources", "swapchain has invalid dimensions (%ux%u)", scDesc.BufferDesc.Width, scDesc.BufferDesc.Height);
            return false;
        }

        Logger::InfoTag("CreateRenderResources", "swapchain size: %ux%u", scDesc.BufferDesc.Width, scDesc.BufferDesc.Height);

        D3D12_DESCRIPTOR_HEAP_DESC srvDesc = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
        Logger::InfoTag("CreateRenderResources", "creating SRV heap");
        if (FAILED(pDevice->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&g_srvHeap)))) {
            Logger::ErrorTag("CreateRenderResources", "failed to create SRV heap");
            return false;
        }
        Logger::InfoTag("CreateRenderResources", "SRV heap = 0x%p", (void*)g_srvHeap);

        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1 };
        Logger::InfoTag("CreateRenderResources", "creating RTV heap");
        if (FAILED(pDevice->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&g_rtvHeap)))) {
            Logger::ErrorTag("CreateRenderResources", "failed to create RTV heap");
            g_srvHeap->Release(); g_srvHeap = nullptr;
            return false;
        }
        Logger::InfoTag("CreateRenderResources", "RTV heap = 0x%p", (void*)g_rtvHeap);

        
        
        
        
        g_rtvHandle[0] = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < FRAME_COUNT; i++) {
            Logger::InfoTag("CreateRenderResources", "creating cmd allocator %u", i);
            if (FAILED(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_cmdAlloc[i])))) {
                Logger::ErrorTag("CreateRenderResources", "failed to create command allocator %u", i);
                DestroyRenderResources();
                return false;
            }
            Logger::InfoTag("CreateRenderResources", "cmdAlloc[%u] = 0x%p", i, (void*)g_cmdAlloc[i]);
        }

        Logger::InfoTag("CreateRenderResources", "creating command list");
        if (FAILED(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_cmdAlloc[0], nullptr, IID_PPV_ARGS(&g_cmdList)))) {
            Logger::ErrorTag("CreateRenderResources", "failed to create command list");
            DestroyRenderResources();
            return false;
        }
        Logger::InfoTag("CreateRenderResources", "cmdList = 0x%p", (void*)g_cmdList);
        g_cmdList->Close();

        Logger::InfoTag("CreateRenderResources", "creating fence");
        if (FAILED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)))) {
            Logger::ErrorTag("CreateRenderResources", "failed to create fence");
            DestroyRenderResources();
            return false;
        }
        Logger::InfoTag("CreateRenderResources", "fence = 0x%p", (void*)g_fence);

        Logger::InfoTag("CreateRenderResources", "done");
        return true;
    }

    
    
    
    static void RenderOverlay(IDXGISwapChain3* pSC3) {
        if (!g_pDevice || !g_cmdQueue || !g_cmdList || !g_fence) return;

        UINT idx = pSC3->GetCurrentBackBufferIndex();

        
        
        
        if (g_fenceValue > 0 && g_fence->GetCompletedValue() < g_fenceValue) {
            HANDLE evt = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (evt) {
                g_fence->SetEventOnCompletion(g_fenceValue, evt);
                DWORD res = WaitForSingleObject(evt, 1000);
                CloseHandle(evt);
                if (res != WAIT_OBJECT_0) {
                    Logger::WarnTag("RenderOverlay", "fence wait timed out (GPU stalled); skipping overlay frame");
                    return;
                }
            }
        }

        if (g_pDevice && g_pDevice->GetDeviceRemovedReason() != S_OK) {
            Logger::WarnTag("RenderOverlay", "device removed (0x%08lX); skipping overlay frame",
                (unsigned long)g_pDevice->GetDeviceRemovedReason());
            return;
        }

        ID3D12Resource* backBuf = nullptr;
        if (FAILED(pSC3->GetBuffer(idx, IID_PPV_ARGS(&backBuf)))) {
            Logger::WarnTag("RenderOverlay", "GetBuffer(%u) failed", idx);
            return;
        }

        
        D3D12_RESOURCE_DESC resDesc = backBuf->GetDesc();
        D3D12_RENDER_TARGET_VIEW_DESC rtvViewDesc{};
        rtvViewDesc.Format = GetNonSRVFormat(resDesc.Format);
        rtvViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        g_pDevice->CreateRenderTargetView(backBuf, &rtvViewDesc, g_rtvHandle[0]);

        const UINT allocIdx = (idx < FRAME_COUNT) ? idx : 0;
        if (FAILED(g_cmdAlloc[allocIdx]->Reset())) {
            backBuf->Release();
            return;
        }
        if (FAILED(g_cmdList->Reset(g_cmdAlloc[allocIdx], nullptr))) {
            backBuf->Release();
            return;
        }

        
        
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = backBuf;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_cmdList->ResourceBarrier(1, &barrier);

        g_cmdList->OMSetRenderTargets(1, &g_rtvHandle[0], FALSE, nullptr);

        ImGuiIO& io = ImGui::GetIO();
        D3D12_VIEWPORT vp{};
        vp.TopLeftX = 0.0f; vp.TopLeftY = 0.0f;
        vp.Width = io.DisplaySize.x; vp.Height = io.DisplaySize.y;
        vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
        g_cmdList->RSSetViewports(1, &vp);
        D3D12_RECT scissor{ 0, 0, (LONG)io.DisplaySize.x, (LONG)io.DisplaySize.y };
        g_cmdList->RSSetScissorRects(1, &scissor);

        
        
        
        
        
        __try {
            float blurFade = 0.0f;
            if (g_clickGui && g_clickGui->IsBlurEnabled())
                blurFade = std::clamp(g_clickGui->GetMenuFade(), 0.0f, 1.0f);
            if (blurFade > 0.001f) {
                float strength = g_clickGui->GetBlurStrength() * blurFade;
                BackgroundBlur::Instance().Render(g_cmdList, backBuf, g_rtvHandle[0],
                    (UINT)resDesc.Width, (UINT)resDesc.Height, resDesc.Format, strength);
                
                g_cmdList->OMSetRenderTargets(1, &g_rtvHandle[0], FALSE, nullptr);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::ErrorTag("RenderOverlay", "background blur failed (code 0x%08lX); skipping",
                (unsigned long)GetExceptionCode());
            g_cmdList->OMSetRenderTargets(1, &g_rtvHandle[0], FALSE, nullptr);
        }

        
        
        
        
        bool frameSubmitted = false;
        bool imFrameOpen = false;
        __try {
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            imFrameOpen = true;

            
            Hooks::Keyboard::Update();

            
            
            
            __try {
                ModuleManager::CallEvent();
                ModuleManager::CallRenderEvent();
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                Logger::ErrorTag("RenderOverlay", "exception in feature draw (code 0x%08lX); continuing frame",
                    (unsigned long)GetExceptionCode());
            }

            ImGui::Render();
            imFrameOpen = false;

            
            
            
            ID3D12DescriptorHeap* heaps[] = { g_srvHeap };
            g_cmdList->SetDescriptorHeaps(1, heaps);

            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_cmdList);

            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            g_cmdList->ResourceBarrier(1, &barrier);

            g_cmdList->Close();
            ID3D12CommandList* cl = g_cmdList;
            g_cmdQueue->ExecuteCommandLists(1, &cl);

            g_fenceValue++;
            g_cmdQueue->Signal(g_fence, g_fenceValue);
            frameSubmitted = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::ErrorTag("RenderOverlay", "exception during overlay frame (code 0x%08lX); skipping frame",
                (unsigned long)GetExceptionCode());
            if (imFrameOpen) {
                
                
                __try { ImGui::EndFrame(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }

        
        
        if (!frameSubmitted) {
            g_fenceValue++;
            if (g_cmdQueue) g_cmdQueue->Signal(g_fence, g_fenceValue);
        }

        backBuf->Release();
    }

    

    static void WINAPI HookedExecCmd(ID3D12CommandQueue* pQueue, UINT num, ID3D12CommandList* const* pp) {
        if (!g_shuttingDown.load(std::memory_order_acquire) && !g_cmdQueue && pQueue) {
            Logger::InfoTag("HookedExecCmd", "captured command queue (0x%p)", (void*)pQueue);
            g_cmdQueue = pQueue;
        }
        o_ExecCmd(pQueue, num, pp);
    }

    
    
    static HRESULT WINAPI PresentImpl(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
        auto* pSC3 = static_cast<IDXGISwapChain3*>(pSwapChain);

        
        if (!g_imguiInit && !g_initInProgress) {
            if (!g_cmdQueue) return o_Present(pSwapChain, SyncInterval, Flags);

            g_initInProgress = true;
            Logger::InfoTag("HookedPresent", "first call, initializing D3D12 + ImGui");

            ID3D12Device* pDevice = nullptr;
            if (SUCCEEDED(pSC3->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&pDevice)))) {
                if (CreateRenderResources(pSC3, pDevice)) {
                    g_pDevice = pDevice;  
                    Logger::InfoTag("HookedPresent", "creating ImGui context");
                    IMGUI_CHECKVERSION();
                    ImGui::CreateContext();
                    ImGuiIO& io = ImGui::GetIO();
                    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

                    Resources::LoadFonts();

                    Logger::InfoTag("HookedPresent", "loaded %zu fonts", Resources::Fonts.size());

                    if (Resources::Fonts.empty()) {
                        Logger::WarnTag("HookedPresent", "no fonts, using ImGui default");
                        io.Fonts->AddFontDefault();
                    }
                    else {
                        
                        
                        io.FontDefault = Resources::Fonts[0];
                        for (size_t i = 0; i < Resources::FontNames.size(); i++) {
                            if (Resources::FontNames[i] == "GoogleSans-Medium") {
                                io.FontDefault = Resources::Fonts[i];
                                break;
                            }
                        }
                    }

                    DXGI_SWAP_CHAIN_DESC scDesc{};
                    pSC3->GetDesc(&scDesc);
                    g_hwnd = scDesc.OutputWindow;
                    g_lastSwapChainWidth = scDesc.BufferDesc.Width;
                    g_lastSwapChainHeight = scDesc.BufferDesc.Height;
                    Logger::InfoTag("HookedPresent", "set g_hwnd = 0x%p, swapchain size = %ux%u", (void*)g_hwnd, g_lastSwapChainWidth, g_lastSwapChainHeight);

                    ImGui_ImplWin32_Init(scDesc.OutputWindow);

                    
                    
                    if (!g_wndHooked && g_hwnd) {
                        g_oWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)OxygenWndProc);
                        g_wndHooked = (g_oWndProc != nullptr);
                        Logger::InfoTag("HookedPresent", "input gate installed (prev wndproc=0x%p)", (void*)g_oWndProc);
                    }

                    
                    InstallCursorHooks();

                    DXGI_FORMAT imguiFmt = GetNonSRVFormat(scDesc.BufferDesc.Format);

                    ImGui_ImplDX12_InitInfo initInfo = {};
                    initInfo.Device = pDevice;
                    initInfo.CommandQueue = g_cmdQueue;
                    initInfo.NumFramesInFlight = FRAME_COUNT;
                    initInfo.RTVFormat = imguiFmt;
                    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
                    initInfo.SrvDescriptorHeap = g_srvHeap;
                    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu) {
                        UINT increment = info->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                        static UINT next = 0;
                        D3D12_CPU_DESCRIPTOR_HANDLE cpu = info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
                        D3D12_GPU_DESCRIPTOR_HANDLE gpu = info->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
                        cpu.ptr += SIZE_T(next * increment);
                        gpu.ptr += UINT64(next * increment);
                        *outCpu = cpu;
                        *outGpu = gpu;
                        next++;
                    };
                    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};

                    if (!ImGui_ImplDX12_Init(&initInfo)) {
                        Logger::ErrorTag("HookedPresent", "ImGui_ImplDX12_Init failed");
                        pDevice->Release();
                        g_initInProgress = false;
                        return o_Present(pSwapChain, SyncInterval, Flags);
                    }

                    if (!ImGui_ImplDX12_CreateDeviceObjects()) {
                        Logger::ErrorTag("HookedPresent", "ImGui_ImplDX12_CreateDeviceObjects failed");
                        pDevice->Release();
                        g_initInProgress = false;
                        return o_Present(pSwapChain, SyncInterval, Flags);
                    }

                    ModuleManager::Initialize();

                    
                    for (auto& f : ModuleManager::FeatureList) {
                        if (f->Name == "ClickGui") { g_clickGui = dynamic_cast<ClickGui*>(f.get()); break; }
                    }

                    
                    BackgroundBlur::Instance().Init(g_pDevice);

                    g_imguiInit = true;
                    Logger::InfoTag("HookedPresent", "initialization complete");
                }
                else {
                    Logger::ErrorTag("HookedPresent", "CreateRenderResources failed during init");
                    pDevice->Release();
                }
            }
            else {
                Logger::ErrorTag("HookedPresent", "GetDevice failed during init");
            }

            g_initInProgress = false;
        }

        if (!g_imguiInit) return o_Present(pSwapChain, SyncInterval, Flags);

        
        static std::chrono::steady_clock::time_point s_lastTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        float dt = (float)std::chrono::duration<double>(now - s_lastTime).count();
        s_lastTime = now;
        if (dt <= 0.0f) dt = 0.016f;
        if (dt > 0.1f) dt = 0.1f;
        ImGui::GetIO().DeltaTime = dt;

        
        UpdateDisplaySize(pSwapChain);

        DXGI_SWAP_CHAIN_DESC scDesc{};
        if (SUCCEEDED(pSC3->GetDesc(&scDesc))) {
            if (scDesc.BufferDesc.Width != g_lastSwapChainWidth ||
                scDesc.BufferDesc.Height != g_lastSwapChainHeight) {
                Logger::InfoTag("HookedPresent", "size changed %ux%u -> %ux%u, stabilizing",
                    g_lastSwapChainWidth, g_lastSwapChainHeight,
                    scDesc.BufferDesc.Width, scDesc.BufferDesc.Height);
                g_resizeStabilizeTimer = RESIZE_STABILIZE_TIME;
                g_lastSwapChainWidth = scDesc.BufferDesc.Width;
                g_lastSwapChainHeight = scDesc.BufferDesc.Height;
            }
        }

        
        
        
        if (g_resizeStabilizeTimer > 0.0f) {
            g_resizeStabilizeTimer -= dt;
            if (g_resizeStabilizeTimer < 0.0f) g_resizeStabilizeTimer = 0.0f;
            return o_Present(pSwapChain, SyncInterval, Flags);
        }

        if (WindowIsHidden())
            return o_Present(pSwapChain, SyncInterval, Flags);

        RenderOverlay(pSC3);

        return o_Present(pSwapChain, SyncInterval, Flags);
    }

    
    
    
    
    
    static HRESULT WINAPI HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
        if (g_shuttingDown.load(std::memory_order_acquire))
            return o_Present(pSwapChain, SyncInterval, Flags);

        g_activeRenders.fetch_add(1, std::memory_order_acq_rel);
        HRESULT hr = PresentImpl(pSwapChain, SyncInterval, Flags);
        g_activeRenders.fetch_sub(1, std::memory_order_acq_rel);
        return hr;
    }

    

    bool Init() {
        Logger::InfoTag("DX12::Init", "creating dummy device and swap chain");

        WNDCLASSEXW wc = { sizeof(wc), CS_HREDRAW | CS_VREDRAW, DefWindowProcW, 0, 0, GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr, L"OxygenDummy", nullptr };
        RegisterClassExW(&wc);
        HWND hWnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPED, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

        ID3D12Device* pDevice = nullptr;
        static const D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0
        };
        bool deviceOk = false;
        for (auto fl : levels) {
            if (SUCCEEDED(D3D12CreateDevice(nullptr, fl, IID_PPV_ARGS(&pDevice)))) {
                Logger::InfoTag("DX12::Init", "created D3D12 device at feature level %d.%d", (fl >> 12) & 0xf, (fl >> 8) & 0xf);
                deviceOk = true;
                break;
            }
        }
        if (!deviceOk) {
            Logger::ErrorTag("DX12::Init", "failed to create D3D12 device at any feature level");
            return false;
        }

        ID3D12CommandQueue* pQueue = nullptr;
        D3D12_COMMAND_QUEUE_DESC cqDesc = { D3D12_COMMAND_LIST_TYPE_DIRECT };
        pDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&pQueue));

        IDXGIFactory4* pFactory = nullptr;
        CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));

        DXGI_SWAP_CHAIN_DESC scDesc = {};
        scDesc.BufferCount = 2;
        scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.OutputWindow = hWnd;
        scDesc.SampleDesc.Count = 1;
        scDesc.Windowed = TRUE;
        scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        IDXGISwapChain* pSwapChain = nullptr;
        pFactory->CreateSwapChain(pQueue, &scDesc, &pSwapChain);

        
        IDXGISwapChain3* pSC3 = nullptr;
        pSwapChain->QueryInterface(IID_PPV_ARGS(&pSC3));
        void** scVtable = *reinterpret_cast<void***>(pSC3);
        void** cqVtable = *reinterpret_cast<void***>(pQueue);
        g_scVtable = scVtable;
        g_cqVtable = cqVtable;

        o_Present = (Present_t)HookVTable(scVtable, 8, (void*)HookedPresent);
        o_ExecCmd = (ExecCmdLists_t)HookVTable(cqVtable, 10, (void*)HookedExecCmd);

        
        
        
        Logger::InfoTag("DX12::Init", "hooks installed (Present=0x%p, ExecCmd=0x%p, Resize hooks disabled)",
            (void*)o_Present, (void*)o_ExecCmd);

        if (pSC3) pSC3->Release();
        pSwapChain->Release(); pQueue->Release(); pDevice->Release(); pFactory->Release();
        DestroyWindow(hWnd); UnregisterClassW(L"OxygenDummy", wc.hInstance);

        bool success = o_Present != nullptr;
        Logger::InfoTag("DX12::Init", "%s", success ? "success" : "failed (Present hook is null)");
        return success;
    }

    void Shutdown() {
        Logger::InfoTag("Shutdown", "begin DX12 teardown");

        
        
        
        g_shuttingDown.store(true, std::memory_order_release);
        {
            ULONGLONG deadline = GetTickCount64() + 3000;
            while (g_activeRenders.load(std::memory_order_acquire) != 0 && GetTickCount64() < deadline)
                Sleep(5);
            if (g_activeRenders.load(std::memory_order_acquire) != 0)
                Logger::WarnTag("Shutdown", "render thread still busy after 3s; continuing teardown");
        }

        
        if (g_wndHooked && g_oWndProc && g_hwnd) {
            SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_oWndProc);
            g_wndHooked = false;
            Logger::InfoTag("Shutdown", "wndproc restored");
        }

        
        
        if (g_cursorHooks) {
            MH_RemoveHook(&SetCursorPos);
            MH_RemoveHook(&ClipCursor);
            MH_Uninitialize();
            g_cursorHooks = false;
            Logger::InfoTag("Shutdown", "cursor hooks removed");
        }

        
        
        
        if (g_scVtable && o_Present)
            HookVTable(g_scVtable, 8, (void*)o_Present);
        if (g_cqVtable && o_ExecCmd)
            HookVTable(g_cqVtable, 10, (void*)o_ExecCmd);
        Logger::InfoTag("Shutdown", "vtable hooks restored");

        
        
        
        Sleep(250);
        Logger::InfoTag("Shutdown", "settle delay done, tearing down GPU stack");

        
        
        
        
        
        
        
        __try {
            Logger::InfoTag("Shutdown", "[5a] background blur teardown");
            BackgroundBlur::Instance().Shutdown();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::ErrorTag("Shutdown", "background blur teardown crashed (code 0x%08lX)", (unsigned long)GetExceptionCode());
        }
        Logger::InfoTag("Shutdown",
            "[5b] ImGui context left resident (deferred; DestroyContext is unstable in this imgui build)");
        __try {
            Logger::InfoTag("Shutdown", "[5c] D3D12 resource teardown");
            DestroyRenderResources();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::ErrorTag("Shutdown", "D3D12 resource teardown crashed (code 0x%08lX)", (unsigned long)GetExceptionCode());
        }
        g_imguiInit = false;

        
        
        
        Sleep(100);
        Logger::InfoTag("Shutdown", "done");
    }

}
