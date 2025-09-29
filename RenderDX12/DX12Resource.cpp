#include "stdafx.h"
#include "DX12Resource.h"
#include "../MiniEngineCore/EngineConfig.h"

DX12Resource::DX12Resource()
{

}

DX12Resource::~DX12Resource()
{

}

void DX12Resource::TransitionState(DX12CommandList* dx12CommandList, D3D12_RESOURCE_STATES newState)
{
	assert(m_resource);
	if (m_currentState == newState) return;

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_resource.Get(), m_currentState, newState);
	dx12CommandList->PushStateTransition(barrier);
	m_currentState = newState;

	return;
}

void DX12ResourceBuffer::CreateConstantBuffer(ID3D12Device* device, uint32_t elementByteSize)
{
	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(elementByteSize);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_resource)));
	m_currentState = D3D12_RESOURCE_STATE_GENERIC_READ;
}

void DX12ResourceBuffer::CreateVertexBuffer(ID3D12Device* device, std::span<const Vertex> vertices, DX12CommandList* dx12CommandList)
{
	// resourcse which do not change during frame upload to GPU in initial time!!

	const UINT64 vertexSize = static_cast<UINT64>(vertices.size()) * sizeof(Vertex);
	if (vertexSize == 0) { m_resource.Reset(); m_uploadBuffer.Reset(); return; }

	CD3DX12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC descBuffer = CD3DX12_RESOURCE_DESC::Buffer(vertexSize);
	// vertex buffer
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&descBuffer,
		D3D12_RESOURCE_STATE_COMMON, //DEFAULT 버퍼의 초기 상태는 무조건 COMMON (작성해도 무시됨)
		nullptr,
		IID_PPV_ARGS(&m_resource)));
	m_currentState = D3D12_RESOURCE_STATE_COMMON;
	TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_COPY_DEST);
	dx12CommandList->RecordResourceStateTransition();

	CD3DX12_HEAP_PROPERTIES heapProp2 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC descBuffer2 = CD3DX12_RESOURCE_DESC::Buffer(vertexSize);
	// upload buffer
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProp2,
		D3D12_HEAP_FLAG_NONE,
		&descBuffer2,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_uploadBuffer.ReleaseAndGetAddressOf())));
	m_uploadBufferCurrentState = D3D12_RESOURCE_STATE_GENERIC_READ;

	CopyAndUploadResource(m_uploadBuffer.Get(), vertices.data(), static_cast<size_t>(vertexSize));
	dx12CommandList->GetCommandList()->CopyBufferRegion(m_resource.Get(), 0, m_uploadBuffer.Get(), 0, vertexSize);
	TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}

void DX12ResourceBuffer::CreateIndexBuffer(ID3D12Device* device, std::span<const uint32_t> indices, DX12CommandList* dx12CommandList)
{
	// resourcse which do not change during frame upload to GPU in initial time!!

	const UINT64 indexSize = static_cast<UINT64>(indices.size()) * sizeof(uint32_t);
	if (indexSize == 0) { m_resource.Reset(); m_uploadBuffer.Reset(); return; }

	CD3DX12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC descBuffer = CD3DX12_RESOURCE_DESC::Buffer(indexSize);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&descBuffer,
		D3D12_RESOURCE_STATE_COMMON, //DEFAULT 버퍼의 초기 상태는 무조건 COMMON (작성해도 무시됨)
		nullptr,
		IID_PPV_ARGS(&m_resource)));
	m_currentState = D3D12_RESOURCE_STATE_COMMON;
	TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_COPY_DEST);
	dx12CommandList->RecordResourceStateTransition();

	CD3DX12_HEAP_PROPERTIES heapProp2 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC descBuffer2 = CD3DX12_RESOURCE_DESC::Buffer(indexSize);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProp2,
		D3D12_HEAP_FLAG_NONE,
		&descBuffer2,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_uploadBuffer.ReleaseAndGetAddressOf())));
	m_uploadBufferCurrentState = D3D12_RESOURCE_STATE_GENERIC_READ;

	CopyAndUploadResource(m_uploadBuffer.Get(), indices.data(), static_cast<size_t>(indexSize));
	dx12CommandList->GetCommandList()->CopyBufferRegion(m_resource.Get(), 0, m_uploadBuffer.Get(), 0, indexSize);
	TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
}

void DX12ResourceBuffer::CopyAndUploadResource(ID3D12Resource* uploadBuffer, const void* sourceAddress, size_t dataSize, CD3DX12_RANGE* readRange)
{
	void* mapped = nullptr;
	ThrowIfFailed(uploadBuffer->Map(0, readRange, &mapped));
	memcpy(mapped, sourceAddress, dataSize);
	uploadBuffer->Unmap(0, nullptr);
}

void DX12ResourceTexture::CopyAndUploadResource(ID3D12Resource* uploadBuffer, const void* sourceAddress, size_t dataSize, CD3DX12_RANGE* readRange)
{
	void* mapped = nullptr;
	ThrowIfFailed(uploadBuffer->Map(0, readRange, &mapped));
	memcpy(mapped, sourceAddress, dataSize);
	uploadBuffer->Unmap(0, nullptr);
}

void DX12ResourceTexture::CreateDepthStencil(
	ID3D12Device* device,
	uint32_t clientWidth,
	uint32_t clientHeight,
	uint32_t multiSampleDescCount,
	DXGI_FORMAT depthStencilFormat)
{
	//create DSDesc
	D3D12_RESOURCE_DESC DSVDesc = {};
	DSVDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DSVDesc.Alignment = 0;
	DSVDesc.Width = clientWidth;
	DSVDesc.Height = clientHeight;
	DSVDesc.DepthOrArraySize = 1;
	DSVDesc.MipLevels = 1;
	DSVDesc.Format = depthStencilFormat;
	DSVDesc.SampleDesc.Count = multiSampleDescCount;
	DSVDesc.SampleDesc.Quality = 0;
	DSVDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	DSVDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear = {};
	optClear.Format = depthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&DSVDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(&m_resource)));
	m_currentState = D3D12_RESOURCE_STATE_COMMON;
}

void DX12ResourceTexture::CreateRenderTarget(
	ID3D12Device* device,
	uint32_t clientWidth,
	uint32_t clientHeight,
	uint32_t multiSampleDescCount,
	DXGI_FORMAT renderTargetFormat)
{
	//create RTVDesc
	D3D12_RESOURCE_DESC RTVDesc = {};
	RTVDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	RTVDesc.Alignment = 0;
	RTVDesc.Width = clientWidth;
	RTVDesc.Height = clientHeight;
	RTVDesc.DepthOrArraySize = 1;
	RTVDesc.MipLevels = 1;
	RTVDesc.Format = renderTargetFormat;
	RTVDesc.SampleDesc.Count = multiSampleDescCount;
	RTVDesc.SampleDesc.Quality = 0;
	RTVDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	RTVDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE optClear = {};
	optClear.Color[0] = EngineConfig::DefaultClearColor[0];
	optClear.Color[1] = EngineConfig::DefaultClearColor[1];
	optClear.Color[2] = EngineConfig::DefaultClearColor[2];
	optClear.Color[3] = EngineConfig::DefaultClearColor[3];
	optClear.Format = renderTargetFormat;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&RTVDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&optClear,
		IID_PPV_ARGS(&m_resource)));
	m_currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void DX12ResourceTexture::CreateTexture(
	ID3D12Device* device,
	DX12CommandList* dx12CommandList,
	TexMetadata* texMetaData,
	ScratchImage* img)
{
	const UINT texWidth = SizeToU32(texMetaData->width);
	const UINT texHeight = SizeToU32(texMetaData->height);
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = (texMetaData->dimension == TEX_DIMENSION_TEXTURE2D) ? D3D12_RESOURCE_DIMENSION_TEXTURE2D : D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	texDesc.Width = texWidth;
	texDesc.Height = texHeight;
	texDesc.DepthOrArraySize = (UINT16)((texMetaData->dimension == TEX_DIMENSION_TEXTURE3D) ? texMetaData->depth : texMetaData->arraySize);
	texDesc.MipLevels = (UINT16)texMetaData->mipLevels;
	texDesc.Format = texMetaData->format;
	texDesc.SampleDesc = { 1, 0 };
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(m_resource.GetAddressOf())));
	m_currentState = D3D12_RESOURCE_STATE_COPY_DEST;

	std::vector<D3D12_SUBRESOURCE_DATA> subResourceData;
	subResourceData.reserve(texMetaData->mipLevels * texMetaData->arraySize);
	for (size_t i = 0; i < texMetaData->arraySize; ++i)
	{
		for (size_t j = 0; j < texMetaData->mipLevels; ++j)
		{
			auto* imgLevel = img->GetImage(j, i, 0);
			D3D12_SUBRESOURCE_DATA tmpSubData{};
			tmpSubData.pData = imgLevel->pixels;
			tmpSubData.RowPitch = imgLevel->rowPitch;
			tmpSubData.SlicePitch = imgLevel->slicePitch;
			subResourceData.push_back(tmpSubData);
		}
	}

	const UINT resourceDataSize = SizeToU32(subResourceData.size());
	UINT64 uploadBytes = GetRequiredIntermediateSize(m_resource.Get(), 0, resourceDataSize);
	CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
	auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBytes);
	ThrowIfFailed(device->CreateCommittedResource(
		&uploadHeap,
		D3D12_HEAP_FLAG_NONE,
		&uploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_uploadBuffer.GetAddressOf())));

	UpdateSubresources(dx12CommandList->GetCommandList(), m_resource.Get(), m_uploadBuffer.Get(), 0, 0, resourceDataSize, subResourceData.data());
	TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	dx12CommandList->RecordResourceStateTransition();
}

void DX12ResourceTexture::CreateMaterialorObjectResource(
	ID3D12Device* device,
	UINT byteSize)
{
	CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);

	CD3DX12_RESOURCE_DESC matDesc{};
	matDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	matDesc.Width = byteSize;
	matDesc.Height = 1;
	matDesc.DepthOrArraySize = 1;
	matDesc.MipLevels = 1;
	matDesc.SampleDesc.Count = 1;
	matDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	matDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&matDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&m_resource)));

	m_currentState = D3D12_RESOURCE_STATE_COMMON;
}

void DX12ResourceTexture::CreateUploadBuffer(ID3D12Device* device, UINT byteSize)
{
	CD3DX12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC descBuffer = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	descBuffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	descBuffer.Width = byteSize;
	descBuffer.Height = 1;
	descBuffer.DepthOrArraySize = 1;
	descBuffer.MipLevels = 1;
	descBuffer.SampleDesc.Count = 1;
	descBuffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ThrowIfFailed(device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&descBuffer,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_uploadBuffer)));
}

void DX12ResourceTexture::CreateShadowResource(
	ID3D12Device* device,
	uint32_t shadowWidth,
	uint32_t shadowHeight,
	DXGI_FORMAT shadowResourceFormat,
	DXGI_FORMAT shadowDSVFormat)
{
	//create RTVDesc
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = shadowWidth;
	texDesc.Height = shadowHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = shadowResourceFormat;
	texDesc.SampleDesc = { 1, 0 };
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearVal = {};
	clearVal.Format = shadowDSVFormat;
	clearVal.DepthStencil.Depth = 1.0f;
	clearVal.DepthStencil.Stencil = 0;

	CD3DX12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearVal,
		IID_PPV_ARGS(&m_resource)));

	m_currentState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
}