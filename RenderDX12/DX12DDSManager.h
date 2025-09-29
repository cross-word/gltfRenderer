#pragma once

#include "stdafx.h"
#include <DirectXTex.h>

#include "D3DUtil.h"
#include "DX12Resource.h"
#include "DX12View.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class DX12DDSManager
{
public:
    DX12DDSManager();
    ~DX12DDSManager();

    void LoadAndCreateDDSResource(
        ID3D12Device* device, 
        DX12CommandList* dx12CommandList,
        const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, 
        const wchar_t* filename,
        const std::string textureName);
    inline DX12ResourceTexture* GetDDSResource() const noexcept { return m_DDSResource.get(); }
    inline DX12View* GetDX12DDSView() const noexcept { return m_DX12DDSView.get(); }
    inline std::string GetDDSTextureName() const noexcept { return m_textureName; }
    inline std::wstring GetDDSFileName() const noexcept { return m_fileName; }
private:
    std::unique_ptr<DX12ResourceTexture> m_DDSResource;
    std::unique_ptr<DX12View> m_DX12DDSView;

    std::string m_textureName;
    std::wstring m_fileName;
};