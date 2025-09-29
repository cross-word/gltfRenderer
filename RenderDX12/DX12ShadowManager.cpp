#include "stdafx.h"
#include "DX12ShadowManager.h"

DX12ShadowManager::DX12ShadowManager()
{

}

DX12ShadowManager::~DX12ShadowManager()
{

}

void DX12ShadowManager::Initialzie(ID3D12Device* device, DX12CommandList* dx12CommandList, D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuHandle, D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle)
{
    m_DX12ShadowResource = std::make_unique<DX12ResourceTexture>();
    m_DX12ShadowResource->CreateShadowResource(
        device,
        m_shadowWidth,
        m_shadowHeight,
        m_shadowResourceFormat,
        m_shadowDepthStencilFormat);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = m_shadowDepthStencilFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    m_DX12ShadowDescriptorView = std::make_unique<DX12View>(
        device,
        EViewType::EDepthStencilView,
        m_DX12ShadowResource.get(),
        dsvCpuHandle,
        nullptr,
        nullptr,
        nullptr,
        &dsvDesc);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = m_shadowShaderResourceFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    m_DX12ShadowResourceView = std::make_unique<DX12View>(
        device,
        EViewType::EShaderResourceView,
        m_DX12ShadowResource.get(),
        srvCpuHandle,
        nullptr,
        &srvDesc);
}