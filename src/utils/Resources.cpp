#include "Resources.hpp"
#include <font_resources.h>
#include <asset_resources.h>
#include "logger.hpp"
#include <Windows.h>
#include <wincodec.h>
#include <imgui.h>
#include <string>
#include <filesystem>
#include <vector>
#include <cstdint>
#include <cstring>

    extern HMODULE g_hModule;

namespace Resources {

    std::vector<ImFont*> Fonts;
    std::vector<std::string> FontNames;

    std::string GetDllDirectory() {
        char path[MAX_PATH];
        GetModuleFileNameA(g_hModule, path, sizeof(path));
        return std::filesystem::path(path).parent_path().string();
    }

    static bool DecodePngFromMemory(const void* data, UINT size, ImTextureData& outTexture) {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool comInitialized = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

        IWICImagingFactory* factory = nullptr;
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
        if (FAILED(hr)) {
            if (comInitialized) CoUninitialize();
            return false;
        }

        IWICStream* stream = nullptr;
        hr = factory->CreateStream(&stream);
        if (FAILED(hr) || !stream) {
            factory->Release();
            if (comInitialized) CoUninitialize();
            return false;
        }

        hr = stream->InitializeFromMemory(reinterpret_cast<WICInProcPointer>(const_cast<void*>(data)), size);
        if (FAILED(hr)) {
            stream->Release();
            factory->Release();
            if (comInitialized) CoUninitialize();
            return false;
        }

        IWICBitmapDecoder* decoder = nullptr;
        hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr) || !decoder) {
            stream->Release();
            factory->Release();
            if (comInitialized) CoUninitialize();
            return false;
        }

        IWICBitmapFrameDecode* frame = nullptr;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr) || !frame) {
            decoder->Release();
            stream->Release();
            factory->Release();
            if (comInitialized) CoUninitialize();
            return false;
        }

        UINT width = 0;
        UINT height = 0;
        frame->GetSize(&width, &height);

        IWICFormatConverter* converter = nullptr;
        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr) || !converter) {
            frame->Release();
            decoder->Release();
            stream->Release();
            factory->Release();
            if (comInitialized) CoUninitialize();
            return false;
        }

        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) {
            converter->Release();
            frame->Release();
            decoder->Release();
            stream->Release();
            factory->Release();
            if (comInitialized) CoUninitialize();
            return false;
        }

        const UINT stride = width * 4u;
        const UINT bufferSize = stride * height;
        std::vector<std::uint8_t> pixels(bufferSize);

        hr = converter->CopyPixels(nullptr, stride, bufferSize, pixels.data());
        if (FAILED(hr)) {
            converter->Release();
            frame->Release();
            decoder->Release();
            stream->Release();
            factory->Release();
            if (comInitialized) CoUninitialize();
            return false;
        }

        outTexture.Create(ImTextureFormat_RGBA32, static_cast<int>(width), static_cast<int>(height));
        std::memcpy(outTexture.Pixels, pixels.data(), bufferSize);
        outTexture.SetStatus(ImTextureStatus_WantCreate);

        converter->Release();
        frame->Release();
        decoder->Release();
        stream->Release();
        factory->Release();
        if (comInitialized) CoUninitialize();

        return true;
    }

    static bool LoadEmbeddedPng(const char* resName, ImTextureData& outTexture) {
        HRSRC hRes = FindResourceA(g_hModule, resName, (LPCSTR)RT_RCDATA);
        if (!hRes) {
            Logger::WarnTag("LoadEmbeddedPng", "FindResourceA failed for '%s' (err=%lu)", resName ? resName : "null", GetLastError());
            return false;
        }

        HGLOBAL hMem = LoadResource(g_hModule, hRes);
        if (!hMem) return false;

        void* data = LockResource(hMem);
        DWORD size = SizeofResource(g_hModule, hRes);
        if (!data || !size) return false;

        return DecodePngFromMemory(data, static_cast<UINT>(size), outTexture);
    }

    bool LoadPngTexture(const std::string& relativeFileName, ImTextureData& outTexture) {
        const char* resName = nullptr;
        for (int i = 0; i < AssetData::g_assetCount; i++) {
            if (relativeFileName == AssetData::g_assets[i].fileName) {
                resName = AssetData::g_assets[i].resName;
                break;
            }
        }

        if (!resName) {
            Logger::WarnTag("LoadPngTexture", "no embedded asset for '%s'", relativeFileName.c_str());
            return false;
        }

        return LoadEmbeddedPng(resName, outTexture);
    }

    static ImFont* LoadEmbeddedFont(const char* resName, const char* displayName) {
        HRSRC hRes = FindResourceA(g_hModule, resName, (LPCSTR)RT_RCDATA);
        if (!hRes) {
            Logger::WarnTag("LoadEmbeddedFont", "FindResourceA failed for '%s' (err=%lu)", resName ? resName : "null", GetLastError());
            return nullptr;
        }

        HGLOBAL hMem = LoadResource(g_hModule, hRes);
        if (!hMem) return nullptr;

        void* data = LockResource(hMem);
        DWORD size = SizeofResource(g_hModule, hRes);
        if (!data || !size) return nullptr;

        ImFont* font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, size, 18.0f);
        if (font) {
            Logger::InfoTag("LoadEmbeddedFont", "loaded '%s'", displayName);
        }
        else {
            Logger::WarnTag("LoadEmbeddedFont", "AddFontFromMemoryTTF failed for '%s'", displayName);
        }
        return font;
    }

    bool LoadFonts() {
        Fonts.clear();
        FontNames.clear();

        for (int i = 0; i < FontData::g_fontCount; i++) {
            const auto& entry = FontData::g_fonts[i];
            ImFont* font = LoadEmbeddedFont(entry.resName, entry.name);
            if (font) {
                Fonts.push_back(font);
                FontNames.push_back(entry.name);
            }
        }

        if (Fonts.empty()) {
            Logger::WarnTag("LoadFonts", "no embedded fonts, will use ImGui default");
        }

        return !Fonts.empty();
    }

}
