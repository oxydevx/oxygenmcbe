#pragma once

#include <d3d12.h>
















class BackgroundBlur {
public:
    static BackgroundBlur& Instance();

    
    
    bool Init(ID3D12Device* device);

    
    
    void Render(ID3D12GraphicsCommandList* cmd,
                ID3D12Resource* backBuffer,
                D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv,
                UINT width, UINT height, DXGI_FORMAT backFormat,
                float strength = 1.0f);

    
    void Shutdown();

private:
    BackgroundBlur() = default;
    BackgroundBlur(const BackgroundBlur&) = delete;
    BackgroundBlur& operator=(const BackgroundBlur&) = delete;

    bool EnsureResources(ID3D12Device* device, DXGI_FORMAT backFormat,
                         DXGI_FORMAT rtvFormat, UINT width, UINT height);
    void ReleaseResources();

    ID3D12Device*              m_device   = nullptr;
    ID3D12RootSignature*       m_rootSig  = nullptr;
    ID3D12PipelineState*       m_pso      = nullptr;
    ID3DBlob*                  m_vsBlob   = nullptr;
    ID3DBlob*                  m_psBlob   = nullptr;

    ID3D12DescriptorHeap*      m_srvHeap  = nullptr; 
    UINT                       m_srvInc   = 0;       
    ID3D12DescriptorHeap*      m_rtvHeap  = nullptr; 
    ID3D12Resource*            m_srcTex   = nullptr; 
    ID3D12Resource*            m_tmpTex   = nullptr; 
    ID3D12Resource*            m_dstTex   = nullptr; 
    D3D12_CPU_DESCRIPTOR_HANDLE m_tmpRtv  = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dstRtv  = {};

    UINT   m_w = 0, m_h = 0;
    DXGI_FORMAT m_rtvFmt = DXGI_FORMAT_UNKNOWN;
    bool   m_ok = false;
    bool   m_initialized = false;

    D3D12_RESOURCE_STATES m_srcState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES m_tmpState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES m_dstState = D3D12_RESOURCE_STATE_COMMON;
};
