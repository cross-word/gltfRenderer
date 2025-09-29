#pragma once

#include "stdafx.h"
#include "DX12Resource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12VIEW
MAIN WORK:
1. create View of DX12Resource
IMPORTANT MEMBER:
1. m_DX12Resource
2. m_resourceView
*/
class DX12View
{
public:

	DX12View(DX12Resource* dx12Resource);
	DX12View(DX12ResourceBuffer* dx12ResourceBuffer);
	DX12View(DX12ResourceTexture* dx12ResourceTexture);
	DX12View(
		ID3D12Device* device,
		EViewType viewType,
		DX12Resource* dx12Resource,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		const D3D12_CONSTANT_BUFFER_VIEW_DESC* cbvDesc = nullptr,
		const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr,
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr,
		const D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc = nullptr,
		const D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc = nullptr);
	DX12View(
		ID3D12Device* device,
		EViewType viewType,
		DX12Resource* dx12Resource,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		D3D12_GPU_VIRTUAL_ADDRESS bufferLocation,
		UINT sizeInBytes,
		UINT vertexStride = 0,
		DXGI_FORMAT indexFormat = DXGI_FORMAT_R8G8B8A8_UNORM);
	~DX12View();

	D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferView();
	D3D12_INDEX_BUFFER_VIEW* GetIndexBufferView();
	D3D12_CONSTANT_BUFFER_VIEW_DESC* GetConstantBufferViewDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC* GetShaderResourceViewDesc();
	D3D12_UNORDERED_ACCESS_VIEW_DESC* GetUnorderedAccessViewDesc();
	D3D12_DEPTH_STENCIL_VIEW_DESC* GetDepthStencilViewDesc();
	D3D12_RENDER_TARGET_VIEW_DESC* GetRenderTargetViewDesc();

	D3D12_CPU_DESCRIPTOR_HANDLE const GetCPUHandle() { return m_cpuHandle; }
private:
	DX12Resource* m_DX12Resource;
	EViewType m_type{ EViewType::EVertexView };
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle{ 0 };

	union ResourceView
	{
		D3D12_VERTEX_BUFFER_VIEW         m_vertexBufferView;
		D3D12_INDEX_BUFFER_VIEW          m_indexBufferView;
		D3D12_CONSTANT_BUFFER_VIEW_DESC  m_constantBufferViewDesc;
		D3D12_SHADER_RESOURCE_VIEW_DESC  m_shaderResourceViewDesc;
		D3D12_UNORDERED_ACCESS_VIEW_DESC m_unorederedAccessViewDesc;
		D3D12_DEPTH_STENCIL_VIEW_DESC    m_depthStencilViewDesc;
		D3D12_RENDER_TARGET_VIEW_DESC    m_renderTargetViewDesc;
	} m_resourceView;

};