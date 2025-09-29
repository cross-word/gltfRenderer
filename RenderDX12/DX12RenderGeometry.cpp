#include "stdafx.h"
#include "DX12RenderGeometry.h"
#include "../FileLoader/SimpleLoader.h"
#include "../FileLoader/GLTFLoader.h"

DX12RenderGeometry::DX12RenderGeometry()
{

}

DX12RenderGeometry::~DX12RenderGeometry()
{

}

bool DX12RenderGeometry::InitMeshFromData(
	ID3D12Device* device,
	DX12CommandList* dx12CommandList,
	const MeshData& meshData,
	D3D12_PRIMITIVE_TOPOLOGY vertexPrimitiveType)
{
	if (meshData.vertices.empty() || meshData.indices.empty())
		return false;

	D3D12_CPU_DESCRIPTOR_HANDLE nullHandle = {};
	const uint32_t vertexBufferSize = (uint32_t)meshData.vertices.size() * sizeof(Vertex);
	const uint32_t vertexStride = sizeof(Vertex);
	const uint32_t indexBufferSize = (uint32_t)meshData.indices.size() * sizeof(uint32_t);

	m_DX12VertexBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12VertexBuffer->CreateVertexBuffer(device, meshData.vertices, dx12CommandList);

	m_DX12IndexBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12IndexBuffer->CreateIndexBuffer(device, meshData.indices, dx12CommandList);

	m_DX12VertexView = std::make_unique<DX12View>(
		device,
		EViewType::EVertexView,
		m_DX12VertexBuffer.get(),
		nullHandle,
		m_DX12VertexBuffer.get()->GetGPUVAdress(),
		vertexBufferSize,
		vertexStride);

	m_DX12IndexView = std::make_unique<DX12View>(
		device,
		EViewType::EIndexView,
		m_DX12IndexBuffer.get(),
		nullHandle,
		m_DX12IndexBuffer.get()->GetGPUVAdress(),
		indexBufferSize,
		0U,
		m_indexFormat);

	m_primitiveTopologyType = vertexPrimitiveType;
	const UINT indexSize = SizeToU32(meshData.indices.size());
	const UINT vertexSize = SizeToU32(meshData.vertices.size());
	m_indexCount = indexSize;
	m_vertexCount = vertexSize;
	return true;
}