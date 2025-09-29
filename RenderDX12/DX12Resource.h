#pragma once

#include <span>

#include "stdafx.h"
#include "DX12CommandList.h"
#include "../FileLoader/SimpleLoader.h"
#include <DirectXTex.h>
using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12RESOURCE
MAIN WORK:
1. interface of DX12ResourceBuffer, DX12ResourceTexture
2. make resource
3. manage Resource State
MAIN MEMBER:
1. m_resource
2. m_currentState
3. TransitionState() ****** Every Resource State must be change through this class
*/
class DX12Resource
{
public:
	DX12Resource();
	~DX12Resource();

	ID3D12Resource* GetResource() const noexcept { return m_resource.Get(); }
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVAdress() const noexcept { return m_resource->GetGPUVirtualAddress(); }
	D3D12_RESOURCE_STATES GetCurrentState() const noexcept { return m_currentState; }

	void TransitionState(DX12CommandList* dx12CommandList, D3D12_RESOURCE_STATES newState);

	void Reset() noexcept { m_resource.Reset(); m_currentState = D3D12_RESOURCE_STATE_COMMON; }
	ID3D12Resource** GetAddressOf() noexcept { return m_resource.GetAddressOf(); }
protected:
	ComPtr<ID3D12Resource> m_resource = nullptr;
	D3D12_RESOURCE_STATES  m_currentState = D3D12_RESOURCE_STATE_COMMON; //default state common
};

class DX12ResourceBuffer : public DX12Resource
{
public:
	void ResetUploadBuffer() noexcept { m_uploadBuffer.Reset(); m_uploadBufferCurrentState = D3D12_RESOURCE_STATE_COMMON; }
	void CreateConstantBuffer(ID3D12Device* device, uint32_t elementByteSize);
    void CreateVertexBuffer(ID3D12Device* device, std::span<const Vertex> vertices, DX12CommandList* dx12CommandList);
	void CreateIndexBuffer(ID3D12Device* device, std::span<const uint32_t> indices, DX12CommandList* dx12CommandList);
	void CopyAndUploadResource(ID3D12Resource* uploadBuffer, const void* sourceAddress, size_t dataSize, CD3DX12_RANGE* readRange = nullptr);

private:
	ComPtr<ID3D12Resource> m_uploadBuffer;
	D3D12_RESOURCE_STATES  m_uploadBufferCurrentState = D3D12_RESOURCE_STATE_COMMON; //default state common
};

class DX12ResourceTexture : public DX12Resource
{
private:

public:
	ID3D12Resource* GetUploadBuffer() const noexcept { return m_uploadBuffer.Get(); }
	void CreateUploadBuffer(ID3D12Device* device, UINT byteSize);
	void ResetUploadBuffer() noexcept { m_uploadBuffer.Reset(); m_uploadBufferCurrentState = D3D12_RESOURCE_STATE_COMMON; }
	void CopyAndUploadResource(ID3D12Resource* uploadBuffer, const void* sourceAddress, size_t dataSize, CD3DX12_RANGE* readRange = nullptr);
	void CreateDepthStencil(
		ID3D12Device* device,
		uint32_t clientWidth,
		uint32_t clientHeight,
		uint32_t multiSampleDescCount,
		DXGI_FORMAT depthStencilFormat);

	void CreateRenderTarget(
		ID3D12Device* device,
		uint32_t clientWidth,
		uint32_t clientHeight,
		uint32_t multiSampleDescCount,
		DXGI_FORMAT renderTargetFormat);

	void CreateTexture(
		ID3D12Device* device,
		DX12CommandList* dx12CommandList,
		TexMetadata* texMetaData,
		ScratchImage* img);

	void CreateMaterialorObjectResource(
		ID3D12Device* device,
		UINT byteSize);

	void CreateShadowResource(
		ID3D12Device* device,
		uint32_t shadowWidth,
		uint32_t shadowHeight,
		DXGI_FORMAT shadowResourceFormat,
		DXGI_FORMAT shadowDSVFormat);

private:
	ComPtr<ID3D12Resource> m_uploadBuffer;
	D3D12_RESOURCE_STATES  m_uploadBufferCurrentState = D3D12_RESOURCE_STATE_COMMON; //default state common
};