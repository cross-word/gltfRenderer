#pragma once
#include <filesystem>
#include <windows.h>

inline std::filesystem::path ExeDir()
{
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
}

inline std::wstring MakePath(const wchar_t* rel)
{
    return (ExeDir() / rel).lexically_normal().wstring();
}


struct EngineConfig
{
    static constexpr UINT DefaultWidth = 800;
    static constexpr UINT DefaultHeight = 600;
    static constexpr float DefaultClearColor[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
    static constexpr UINT MsaaSampleCount = 8;
    static constexpr LONGLONG TargetFrameTime = 16666; // 16.666ms
    static constexpr UINT SwapChainBufferCount = 3;
    static constexpr UINT ConstantBufferCount = 1;
    static constexpr UINT NumDefaultObjectSRVSlot = 65536; //16-bit
    static inline constexpr const wchar_t* ModelObjFilePath[] = {
        LR"(..\..\Models\Skull.obj)",
        LR"(..\..\Models\Car.obj)"
    }; // tmp
    static inline constexpr const wchar_t* DDSFilePath[] = {
        LR"(..\..\Textures\bricks.dds)",
        LR"(..\..\Textures\bricks2.dds)",
        LR"(..\..\Textures\bricks3.dds)",
    }; // tmp
    static inline const std::wstring kShaderPathS = MakePath(LR"(..\..\..\Shaders\Default.hlsl)");
    static inline const std::wstring kShadowShaderPathS = MakePath(LR"(..\..\..\Shaders\Shadows.hlsl)");
    static inline const std::wstring kScenePathS = MakePath(LR"(..\..\..\..\main_sponza\NewSponza_Main_glTF_003.gltf)");
    //static inline const std::wstring kScenePathS = MakePath(LR"(D:\MiniEngine\scene\cozy\cozy.gltf)");
    static inline const std::wstring kShaderRayTracingPathS = MakePath(LR"(..\..\..\Shaders\RayTracing.hlsl)");
    static inline const std::wstring kShaderDirectoryPathS = MakePath(LR"(..\..\..\Shaders)");
    static inline const wchar_t* ShaderFilePath = kShaderPathS.c_str();
    static inline const wchar_t* ShadowShaderFilePath = kShadowShaderPathS.c_str();
    static inline const wchar_t* SceneFilePath = kScenePathS.c_str();
    static inline const wchar_t* ShaderRayTracingPath = kShaderRayTracingPathS.c_str();
    static inline const wchar_t* ShaderDirectoryPath = kShaderDirectoryPathS.c_str();


    static constexpr UINT NumThreadWorker = 4;
    static constexpr bool UseMultiThread = false;
    static constexpr UINT MaxTextureCount = 200;

};