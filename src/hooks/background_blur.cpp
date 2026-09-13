#include "background_blur.hpp"

#include <d3dcompiler.h>
#include <cstring>

#include "../utils/logger.hpp"

#pragma comment(lib, "d3dcompiler")

namespace {



const char* kBlurVS = R"HLSL(
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VSOut VSMain(uint vid : SV_VertexID) {
    float2 p = float2((vid == 1) ? 3.0f : -1.0f, (vid == 2) ? 3.0f : -1.0f);
    VSOut o;
    o.pos = float4(p, 0.0f, 1.0f);
    o.uv  = float2(p.x * 0.5f + 0.5f, 0.5f - p.y * 0.5f);
    return o;
}
)HLSL";



const char* kBlurPS = R"HLSL(
Texture2D g_Tex : register(t0);
SamplerState g_Samp : register(s0);
cbuffer Params : register(b0) { float2 g_Texel; };
float4 PSMain(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_Target {
    float w0 = 0.227027f, w1 = 0.1945946f, w2 = 0.1216216f, w3 = 0.054054f, w4 = 0.016216f;
    float3 c  = g_Tex.Sample(g_Samp, uv).rgb * w0;
    c += g_Tex.Sample(g_Samp, uv + g_Texel * 1.0f).rgb * w1;
    c += g_Tex.Sample(g_Samp, uv - g_Texel * 1.0f).rgb * w1;
    c += g_Tex.Sample(g_Samp, uv + g_Texel * 2.0f).rgb * w2;
    c += g_Tex.Sample(g_Samp, uv - g_Texel * 2.0f).rgb * w2;
    c += g_Tex.Sample(g_Samp, uv + g_Texel * 3.0f).rgb * w3;
    c += g_Tex.Sample(g_Samp, uv - g_Texel * 3.0f).rgb * w3;
    c += g_Tex.Sample(g_Samp, uv + g_Texel * 4.0f).rgb * w4;
    c += g_Tex.Sample(g_Samp, uv - g_Texel * 4.0f).rgb * w4;
    return float4(c, 1.0f);
}
)HLSL";

DXGI_FORMAT NonSRVFormat(DXGI_FORMAT fmt) {
    return fmt == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM : fmt;
}


static bool s_loggedInitFail   = false;
static bool s_loggedResFail    = false;
static bool s_loggedFirstFrame = false;

} 

BackgroundBlur& BackgroundBlur::Instance() {
    static BackgroundBlur inst;
    return inst;
}

bool BackgroundBlur::Init(ID3D12Device* device) {
    if (m_initialized) return true;
    if (!device) return false;
    m_device = device;

    ID3DBlob* err = nullptr;
    HRESULT hr = D3DCompile(kBlurVS, std::strlen(kBlurVS), "blur_vs", nullptr, nullptr,
                            "VSMain", "vs_5_0", 0, 0, &m_vsBlob, &err);
    if (FAILED(hr)) {
        if (err) { Logger::ErrorTag("BackgroundBlur", "VS compile: %s", (const char*)err->GetBufferPointer()); err->Release(); }
        return false;
    }
    hr = D3DCompile(kBlurPS, std::strlen(kBlurPS), "blur_ps", nullptr, nullptr,
                    "PSMain", "ps_5_0", 0, 0, &m_psBlob, &err);
    if (FAILED(hr)) {
        if (err) { Logger::ErrorTag("BackgroundBlur", "PS compile: %s", (const char*)err->GetBufferPointer()); err->Release(); }
        return false;
    }

    
    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER params[2] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[0].DescriptorTable.NumDescriptorRanges = 1;
    params[0].DescriptorTable.pDescriptorRanges = &srvRange;

    
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[1].Constants.ShaderRegister = 0;
    params[1].Constants.RegisterSpace = 0;
    params[1].Constants.Num32BitValues = 2;

    D3D12_STATIC_SAMPLER_DESC samp = {};
    samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samp.ShaderRegister = 0;
    samp.RegisterSpace = 0;
    samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters = 2;
    rsDesc.pParameters = params;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers = &samp;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ID3DBlob* rsBlob = nullptr;
    hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &err);
    if (FAILED(hr)) {
        if (err) { Logger::ErrorTag("BackgroundBlur", "root sig serialize: %s", (const char*)err->GetBufferPointer()); err->Release(); }
        return false;
    }
    hr = device->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig));
    if (rsBlob) rsBlob->Release();
    if (FAILED(hr)) {
        Logger::ErrorTag("BackgroundBlur", "CreateRootSignature failed (0x%08lX)", (unsigned long)hr);
        return false;
    }
    Logger::InfoTag("BackgroundBlur", "Init OK (root signature + shaders compiled)");

    m_initialized = true;
    Logger::InfoTag("BackgroundBlur", "initialized (shaders + root signature)");
    return true;
}

bool BackgroundBlur::EnsureResources(ID3D12Device* device, DXGI_FORMAT backFormat,
                                     DXGI_FORMAT rtvFormat, UINT width, UINT height) {
    if (m_ok && m_w == width && m_h == height && m_rtvFmt == rtvFormat)
        return true;

    ReleaseResources();

    m_srvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = 3;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_srvHeap)))) {
        Logger::ErrorTag("BackgroundBlur", "EnsureResources: CreateDescriptorHeap(SRV) failed");
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = 2;
    if (FAILED(device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvHeap)))) {
        Logger::ErrorTag("BackgroundBlur", "EnsureResources: CreateDescriptorHeap(RTV) failed");
        return false;
    }

    auto mkTex = [&](DXGI_FORMAT fmt, D3D12_RESOURCE_FLAGS flags, ID3D12Resource** out) -> bool {
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = width;
        d.Height = height;
        d.DepthOrArraySize = 1;
        d.MipLevels = 1;
        d.Format = fmt;
        d.SampleDesc.Count = 1;
        d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        d.Flags = flags;
        D3D12_HEAP_PROPERTIES hp{};
        hp.Type = D3D12_HEAP_TYPE_DEFAULT;
        return SUCCEEDED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(out)));
    };

    
    if (!mkTex(backFormat, D3D12_RESOURCE_FLAG_NONE, &m_srcTex)) {
        Logger::ErrorTag("BackgroundBlur", "EnsureResources: create srcTex (%d) failed", (int)backFormat);
        return false;
    }
    if (!mkTex(rtvFormat, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &m_tmpTex)) {
        Logger::ErrorTag("BackgroundBlur", "EnsureResources: create tmpTex failed");
        return false;
    }
    if (!mkTex(rtvFormat, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &m_dstTex)) {
        Logger::ErrorTag("BackgroundBlur", "EnsureResources: create dstTex failed");
        return false;
    }

    
    D3D12_CPU_DESCRIPTOR_HANDLE srvBase = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    auto writeSrv = [&](ID3D12Resource* tex, UINT index, DXGI_FORMAT fmt) {
        D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = fmt;
        sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sd.Texture2D.MipLevels = 1;
        sd.Texture2D.MostDetailedMip = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE h = srvBase;
        h.ptr += SIZE_T(index) * m_srvInc;
        device->CreateShaderResourceView(tex, &sd, h);
    };
    writeSrv(m_srcTex, 0, backFormat);
    writeSrv(m_tmpTex, 1, rtvFormat);
    writeSrv(m_dstTex, 2, rtvFormat);

    
    D3D12_RENDER_TARGET_VIEW_DESC rtvView{};
    rtvView.Format = rtvFormat;
    rtvView.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    D3D12_CPU_DESCRIPTOR_HANDLE h0 = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT rtvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    device->CreateRenderTargetView(m_tmpTex, &rtvView, h0);
    m_tmpRtv = h0;
    h0.ptr += SIZE_T(rtvInc);
    device->CreateRenderTargetView(m_dstTex, &rtvView, h0);
    m_dstRtv = h0;

    D3D12_BLEND_DESC blend{};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    blend.RenderTarget[0].BlendEnable = FALSE;
    blend.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rast{};
    rast.FillMode = D3D12_FILL_MODE_SOLID;
    rast.CullMode = D3D12_CULL_MODE_NONE;
    rast.DepthClipEnable = TRUE;

    D3D12_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = FALSE;
    ds.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = m_rootSig;
    pso.VS = { m_vsBlob->GetBufferPointer(), m_vsBlob->GetBufferSize() };
    pso.PS = { m_psBlob->GetBufferPointer(), m_psBlob->GetBufferSize() };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = rtvFormat;
    pso.SampleDesc.Count = 1;
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState = rast;
    pso.BlendState = blend;
    pso.DepthStencilState = ds;

    HRESULT hrPso = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso));
    if (FAILED(hrPso)) {
        Logger::ErrorTag("BackgroundBlur", "CreateGraphicsPipelineState failed (0x%08lX), rtvFmt=%d",
            (unsigned long)hrPso, (int)rtvFormat);
        ReleaseResources();
        return false;
    }

    m_w = width; m_h = height; m_rtvFmt = rtvFormat;
    m_srcState = D3D12_RESOURCE_STATE_COMMON;
    m_tmpState = D3D12_RESOURCE_STATE_COMMON;
    m_dstState = D3D12_RESOURCE_STATE_COMMON;
    m_ok = true;
    return true;
}

void BackgroundBlur::ReleaseResources() {
    if (m_pso)     { m_pso->Release();     m_pso = nullptr; }
    if (m_srvHeap) { m_srvHeap->Release(); m_srvHeap = nullptr; }
    if (m_rtvHeap) { m_rtvHeap->Release(); m_rtvHeap = nullptr; }
    if (m_srcTex)  { m_srcTex->Release();  m_srcTex = nullptr; }
    if (m_tmpTex)  { m_tmpTex->Release();  m_tmpTex = nullptr; }
    if (m_dstTex)  { m_dstTex->Release();  m_dstTex = nullptr; }
    m_tmpRtv = {};
    m_dstRtv = {};
    m_ok = false;
    m_w = m_h = 0;
    m_rtvFmt = DXGI_FORMAT_UNKNOWN;
    m_srcState = m_tmpState = m_dstState = D3D12_RESOURCE_STATE_COMMON;
}

void BackgroundBlur::Render(ID3D12GraphicsCommandList* cmd,
                            ID3D12Resource* backBuffer,
                            D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv,
                            UINT width, UINT height, DXGI_FORMAT backFormat,
                            float strength) {
    if (!m_initialized || !cmd || !backBuffer || !m_device) {
        if (!s_loggedInitFail) {
            s_loggedInitFail = true;
            Logger::ErrorTag("BackgroundBlur",
                "Render skipped: initialized=%d device=%d (blur will not show)",
                (int)m_initialized, (int)(m_device != nullptr));
        }
        return;
    }
    if (width == 0 || height == 0) return;

    const DXGI_FORMAT rtvFormat = NonSRVFormat(backFormat);
    if (!EnsureResources(m_device, backFormat, rtvFormat, width, height)) {
        if (!s_loggedResFail) {
            s_loggedResFail = true;
            Logger::ErrorTag("BackgroundBlur",
                "EnsureResources failed for %ux%u fmt=%d (blur will not show this session)",
                width, height, (int)backFormat);
        }
        return;
    }

    auto transition = [&](ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        if (before == after) return;
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = res;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = before;
        b.Transition.StateAfter = after;
        cmd->ResourceBarrier(1, &b);
    };

    D3D12_GPU_DESCRIPTOR_HANDLE srvBase = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    auto srvAt = [&](UINT index) {
        D3D12_GPU_DESCRIPTOR_HANDLE h = srvBase;
        h.ptr += SIZE_T(index) * m_srvInc;
        return h;
    };

    auto drawPass = [&](UINT srvIndex, D3D12_CPU_DESCRIPTOR_HANDLE rtv, float tx, float ty) {
        cmd->SetPipelineState(m_pso);
        cmd->SetGraphicsRootSignature(m_rootSig);
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd->SetDescriptorHeaps(1, &m_srvHeap);
        cmd->SetGraphicsRootDescriptorTable(0, srvAt(srvIndex));
        float tc[2] = { tx, ty };
        cmd->SetGraphicsRoot32BitConstants(1, 2, tc, 0);
        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        cmd->DrawInstanced(3, 1, 0, 0);
    };

    
    transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    transition(m_srcTex, m_srcState, D3D12_RESOURCE_STATE_COPY_DEST);
    cmd->CopyResource(m_srcTex, backBuffer);
    transition(m_srcTex, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(backBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_srcState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    
    transition(m_tmpTex, m_tmpState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    drawPass(0, m_tmpRtv, (strength / (float)width), 0.0f);
    transition(m_tmpTex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_tmpState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    
    transition(m_dstTex, m_dstState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    drawPass(1, m_dstRtv, 0.0f, (strength / (float)height));
    transition(m_dstTex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_dstState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    
    drawPass(2, backBufferRtv, 0.0f, 0.0f);

    if (!s_loggedFirstFrame) {
        s_loggedFirstFrame = true;
        Logger::InfoTag("BackgroundBlur", "first blur frame drawn (%ux%u, fmt=%d, strength=%.2f)",
            width, height, (int)backFormat, strength);
    }
}

void BackgroundBlur::Shutdown() {
    ReleaseResources();
    if (m_rootSig) { m_rootSig->Release(); m_rootSig = nullptr; }
    if (m_vsBlob)  { m_vsBlob->Release();  m_vsBlob = nullptr; }
    if (m_psBlob)  { m_psBlob->Release();  m_psBlob = nullptr; }
    m_device = nullptr;
    m_initialized = false;
}
