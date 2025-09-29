#pragma once

#include "stdafx.h"

#include "DX12Resource.h"
#include "DX12View.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class DX12RenderGeometry
{
public:
	DX12RenderGeometry();
	~DX12RenderGeometry();

	DX12RenderGeometry operator=(DX12RenderGeometry& moveGeo)
	{
		m_DX12VertexBuffer = moveGeo.m_DX12VertexBuffer;
		m_DX12VertexView = moveGeo.m_DX12VertexView;
		m_DX12IndexBuffer = moveGeo.m_DX12IndexBuffer;
		m_DX12IndexView = moveGeo.m_DX12IndexView;
		m_indexFormat = moveGeo.m_indexFormat;
		m_primitiveTopologyType = moveGeo.m_primitiveTopologyType;
		m_indexCount = moveGeo.m_indexCount;
		m_vertexCount = moveGeo.m_vertexCount;
	}

	bool InitMeshFromData(
		ID3D12Device* device,
		DX12CommandList* dx12CommandList,
		const MeshData& meshData,
		D3D12_PRIMITIVE_TOPOLOGY vertexPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	inline DX12ResourceBuffer* GetDX12VertexBuffer() const noexcept { return m_DX12VertexBuffer.get(); }
	inline DX12ResourceBuffer* GetDX12IndexBuffer() const noexcept { return m_DX12IndexBuffer.get(); }
	inline DX12View* GetDX12VertexBufferView() const noexcept { return m_DX12VertexView.get(); }
	inline DX12View* GetDX12IndexBufferView() const noexcept { return m_DX12IndexView.get(); }

	inline D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveTopologyType() const { return m_primitiveTopologyType; }
	inline UINT GetIndexCount() const noexcept { return m_indexCount; }
	inline UINT GetVertexCount() const noexcept { return m_vertexCount; }

private:
	//vertex
	//index
	std::shared_ptr<DX12ResourceBuffer> m_DX12VertexBuffer;
	std::shared_ptr<DX12View> m_DX12VertexView;

	std::shared_ptr<DX12ResourceBuffer> m_DX12IndexBuffer;
	std::shared_ptr<DX12View> m_DX12IndexView;
	DXGI_FORMAT m_indexFormat = DXGI_FORMAT_R32_UINT;

	//rendering info
	D3D12_PRIMITIVE_TOPOLOGY m_primitiveTopologyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; //default
	UINT m_indexCount = 0;
	UINT m_vertexCount = 0;
};

namespace Render
{
	struct RenderItem
	{
	public:
		void SetRenderGeometry(DX12RenderGeometry* dx12RenderGeoetry) { m_DX12RenderGeomery = dx12RenderGeoetry; }
		void SetTextureIndex(UINT index) { m_textureIndex = index; }
		void SetMaterialIndex(UINT index) { m_materialIndex = index; }
		void SetStartIndexLocation(UINT index) { m_startIndexLocation = index; }
		void SetBaseVertexLocation(UINT index) { m_baseVertexLocation = index; }
		void SetObjWorldMatrix(const XMFLOAT4X4& worldMatrix) { m_objectConst.World = worldMatrix; m_bObjectDirty = true; }
		void SetObjWorldInverseTransposeMatrix(const XMFLOAT4X4& worldMatrix) { m_objectConst.WorldInverseTranspose = worldMatrix; m_bObjectDirty = true; }
		void SetObjTransformMatrix(const XMFLOAT4X4& transformMatrix) { m_objectConst.TexTransform = transformMatrix; m_bObjectDirty = true; }
		void SetObjConstantIndex(UINT index) { m_objectIndex = index; }
		void SetDirtyFlag(bool bdirty) { m_bObjectDirty = bdirty; }

		DX12RenderGeometry* GetRenderGeometry() { return m_DX12RenderGeomery; }
		UINT GetTextureIndex() { return m_textureIndex; }
		UINT GetMaterialIndex() { return m_materialIndex; }
		UINT GetStartIndexLocation() { return m_startIndexLocation; }
		UINT GetBaseVertexLocation() { return m_baseVertexLocation; }
		ObjectConstants& GetObjectConstant() { return m_objectConst; }
		bool IsObjectDirty() { return m_bObjectDirty; }
		UINT GetObjectConstantIndex() { return m_objectIndex; }
	private:
		DX12RenderGeometry* m_DX12RenderGeomery;
		ObjectConstants m_objectConst;

		UINT m_textureIndex = 0;
		UINT m_materialIndex = 0;
		UINT m_startIndexLocation = 0; // + previous index size
		UINT m_baseVertexLocation = 0; // + previous vertex size
		UINT m_objectIndex = 0;
		bool m_bObjectDirty = true;
	};
}