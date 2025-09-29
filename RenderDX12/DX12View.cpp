#include "stdafx.h"
#include "DX12View.h"

DX12View::DX12View(DX12Resource* dx12Resource)
{
	this->m_DX12Resource = dx12Resource;
}

DX12View::DX12View(DX12ResourceBuffer* dx12ResourceBuffer)
{
	this->m_DX12Resource = dx12ResourceBuffer;
}

DX12View::DX12View(DX12ResourceTexture* dx12ResourceTexture)
{
	this->m_DX12Resource = dx12ResourceTexture;
}

DX12View::DX12View(
    ID3D12Device* device,
    EViewType viewType,
    DX12Resource* dx12Resource,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    const D3D12_CONSTANT_BUFFER_VIEW_DESC* cbvDesc,
    const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc,
    const D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc,
    const D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc)
    : m_type(viewType), m_DX12Resource(dx12Resource), m_cpuHandle(cpuHandle)
{
    switch (viewType)
    {
    case EViewType::EConstantBufferView:
        m_resourceView.m_constantBufferViewDesc = *cbvDesc;
        device->CreateConstantBufferView(&m_resourceView.m_constantBufferViewDesc, m_cpuHandle);
        break;
    case EViewType::EShaderResourceView:
        m_resourceView.m_shaderResourceViewDesc = *srvDesc;
        device->CreateShaderResourceView(dx12Resource->GetResource(), &m_resourceView.m_shaderResourceViewDesc, m_cpuHandle);
        break;
    case EViewType::EUnorderedAccessView:
        m_resourceView.m_unorederedAccessViewDesc = *uavDesc;
        device->CreateUnorderedAccessView(dx12Resource->GetResource(), nullptr, &m_resourceView.m_unorederedAccessViewDesc, m_cpuHandle);
        break;
    case EViewType::EDepthStencilView:
        if (dsvDesc != nullptr)
        {
            m_resourceView.m_depthStencilViewDesc = *dsvDesc;
            device->CreateDepthStencilView(dx12Resource->GetResource(), &m_resourceView.m_depthStencilViewDesc, m_cpuHandle);
        }
        else
        {
            device->CreateDepthStencilView(dx12Resource->GetResource(), nullptr, m_cpuHandle);
        }
        break;
    case EViewType::ERenderTargetView:
        if (rtvDesc != nullptr)
        {
            m_resourceView.m_renderTargetViewDesc = *rtvDesc;
            device->CreateRenderTargetView(dx12Resource->GetResource(), &m_resourceView.m_renderTargetViewDesc, m_cpuHandle);
        }
        else 
        {
            device->CreateRenderTargetView(dx12Resource->GetResource(), nullptr, m_cpuHandle);
        }
        break;
    default:
        assert(false && "Wrong constructor for this view type");
    }
}

//vertex view or index view
DX12View::DX12View(
    ID3D12Device* device,
    EViewType viewType,
    DX12Resource* dx12Resource,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_GPU_VIRTUAL_ADDRESS bufferLocation,
    UINT sizeInBytes,
    UINT vertexStride,
    DXGI_FORMAT indexFormat)
    : m_type(viewType), m_DX12Resource(dx12Resource), m_cpuHandle(cpuHandle)
{
    switch (viewType)
    {
    case EViewType::EVertexView:
        m_resourceView.m_vertexBufferView.BufferLocation = dx12Resource->GetResource()->GetGPUVirtualAddress();
        m_resourceView.m_vertexBufferView.SizeInBytes = sizeInBytes;
        m_resourceView.m_vertexBufferView.StrideInBytes = vertexStride;
        break;
    case EViewType::EIndexView:
        m_resourceView.m_indexBufferView.BufferLocation = dx12Resource->GetResource()->GetGPUVirtualAddress();
        m_resourceView.m_indexBufferView.SizeInBytes = sizeInBytes;
        m_resourceView.m_indexBufferView.Format = indexFormat;
        break;
    default:
        assert(false && "Wrong constructor for this view type");
    }
}

DX12View::~DX12View()
{

}

D3D12_VERTEX_BUFFER_VIEW* DX12View::GetVertexBufferView()
{
    assert(&(m_resourceView.m_vertexBufferView) != nullptr);
    return &(m_resourceView.m_vertexBufferView);
}

D3D12_INDEX_BUFFER_VIEW* DX12View::GetIndexBufferView()
{
    assert(&(m_resourceView.m_indexBufferView) != nullptr);
    return &(m_resourceView.m_indexBufferView);
}

D3D12_CONSTANT_BUFFER_VIEW_DESC* DX12View::GetConstantBufferViewDesc()
{
    assert(&(m_resourceView.m_constantBufferViewDesc) != nullptr);
    return &(m_resourceView.m_constantBufferViewDesc);
}

D3D12_SHADER_RESOURCE_VIEW_DESC* DX12View::GetShaderResourceViewDesc()
{
    assert(&(m_resourceView.m_shaderResourceViewDesc) != nullptr);
    return &(m_resourceView.m_shaderResourceViewDesc);
}

D3D12_UNORDERED_ACCESS_VIEW_DESC* DX12View::GetUnorderedAccessViewDesc()
{
    assert(&(m_resourceView.m_unorederedAccessViewDesc) != nullptr);
    return &(m_resourceView.m_unorederedAccessViewDesc);
}

D3D12_DEPTH_STENCIL_VIEW_DESC* DX12View::GetDepthStencilViewDesc()
{
    assert(&(m_resourceView.m_depthStencilViewDesc) != nullptr);
    return &(m_resourceView.m_depthStencilViewDesc);
}

D3D12_RENDER_TARGET_VIEW_DESC* DX12View::GetRenderTargetViewDesc()
{
    assert(&(m_resourceView.m_renderTargetViewDesc) != nullptr);
    return &(m_resourceView.m_renderTargetViewDesc);
}