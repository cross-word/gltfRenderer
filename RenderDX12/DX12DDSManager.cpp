#include "stdafx.h"
#include "DX12DDSManager.h"

DX12DDSManager::DX12DDSManager()
{

}

DX12DDSManager::~DX12DDSManager()
{

}

void DX12DDSManager::LoadAndCreateDDSResource(
    ID3D12Device* device, 
    DX12CommandList* dx12CommandList,
    const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, 
    const wchar_t* filename,
    const std::string textureName)
{
    //resource
    TexMetadata meta;
    ScratchImage img;
    ThrowIfFailed(LoadFromDDSFile(filename, DDS_FLAGS_NONE, &meta, img));
    m_DDSResource = std::make_unique<DX12ResourceTexture>();
    m_DDSResource->CreateTexture(
        device,
        dx12CommandList,
        &meta,
        &img
    );

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_DDSResource->GetResource()->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = m_DDSResource->GetResource()->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    //view
    m_DX12DDSView = std::make_unique<DX12View>(
        device,
        EViewType::EShaderResourceView,
        m_DDSResource.get(),
        *cpuHandle,
        nullptr,
        &srvDesc
    );

    m_fileName = filename;
    m_textureName = textureName;
}

void DX12DDSManager::LoadAndCreateDDSCubeResource(
    ID3D12Device* device,
    DX12CommandList* dx12CommandList,
    const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
    const wchar_t* filename,
    const std::string textureName)
{
    ScratchImage img; 
    TexMetadata meta;
    ThrowIfFailed(LoadFromDDSFile(filename, DDS_FLAGS_NONE, &meta, img));
    m_DDSResource = std::make_unique<DX12ResourceTexture>();
    m_DDSResource->CreateTexture(
        device,
        dx12CommandList,
        &meta,
        &img);
    //m_DDSResource->TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // SRV: TextureCube
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = UINT(meta.mipLevels);
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

    m_DX12DDSView = std::make_unique<DX12View>(
        device,
        EViewType::EShaderResourceView,
        m_DDSResource.get(),
        *cpuHandle,
        nullptr,
        &srvDesc);

    m_fileName = filename;
    m_textureName = textureName;
}