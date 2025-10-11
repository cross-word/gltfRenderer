#pragma once

#include "stdafx.h"
#include "DX12Resource.h"
#include "DX12RenderGeometry.h"
#include "DX12DescriptorHeap.h"

struct GeometryMetadataCPU {
	uint32_t IndexOffset;
	uint32_t VertexOffset;
	uint32_t MaterialIndex;
	uint32_t _pad; // 16byte align
};


class DX12RayTracingManager
{
public:
	DX12RayTracingManager();
	~DX12RayTracingManager();

	ID3D12Resource* GetBLAS(UINT index) const noexcept { return m_BLAS[index]->GetResource(); }
	ID3D12Resource* GetTLAS() const noexcept { return m_TLAS->GetResource(); }
	ID3D12StateObject* GetStateObject() const noexcept { return m_RTState.Get(); }
	ID3D12StateObjectProperties* GetStateObjectProperties() const noexcept { return m_RTProps.Get(); }
	ID3D12Resource* GetRayGenShaderTable() const noexcept { return m_shaderTableRayGen->GetResource(); }
	ID3D12Resource* GetMissShaderTable() const noexcept { return m_shaderTableMiss->GetResource(); }
	ID3D12Resource* GetHitShaderTable() const noexcept { return m_shaderTableHit->GetResource(); }
	DX12ResourceTexture* GetRayOut0() const noexcept { return m_rayOutput.get(); }
	DX12ResourceTexture* GetRayOut1() const noexcept { return m_rayOutput1.get(); }

public:
	void InitBLAS(
		ID3D12Device5* device,
		DX12CommandList* dx12CommandList,
		const std::vector<std::unique_ptr<DX12RenderGeometry>>& dx12RenderGeometry);
	void InitTLAS(
		ID3D12Device5* device,
		DX12CommandList* dx12CommandList,
		const std::vector<std::unique_ptr<DX12RenderGeometry>>& dx12RenderGeometry,
		std::vector<Render::RenderItem>& renderItem);
	void InitRayTracingPipeline(ID3D12Device5* device, ID3D12RootSignature* globalRootSignature, std::vector<std::string>& shaderMacros);
	void CreateShaderTable(ID3D12Device5* device);
	void InitRayOut(ID3D12Device5* device, DX12CommandList* dx12CommandList, DX12DescriptorHeap* dx12DescriptorHeap, UINT width, UINT height);
	void BuildRayGeometryBuffers(
		ID3D12Device5* device,
		DX12CommandList* dx12CommandList,
		DX12DescriptorHeap* dx12DescriptorHeap,
		const std::vector<uint32_t>& indices,
		const std::vector<XMFLOAT3>& positions,
		const std::vector<XMFLOAT3>& normals,
		const std::vector<XMFLOAT2>& texcoords,
		const std::vector<XMFLOAT4>& tangents,
		const std::vector<GeometryMetadataCPU>& geoTable);

private:
	void WriteSceneBufferSRV(ID3D12Device5* device, DX12DescriptorHeap* dx12DescriptorHeap);
private:
	std::vector<std::unique_ptr<DX12ResourceBuffer>> m_BLAS;
	std::vector<std::unique_ptr<DX12ResourceBuffer>> m_BLASScratch;
	std::unique_ptr<DX12ResourceBuffer> m_TLAS;
	std::unique_ptr<DX12ResourceBuffer> m_TLASScratch;
	std::unique_ptr<DX12ResourceBuffer> m_instanceDesc;
	std::unique_ptr<DX12View> m_TLASView;

	// RTX pipeline
	ComPtr<ID3D12StateObject> m_RTState;
	ComPtr<ID3D12StateObjectProperties> m_RTProps;

	// shader table
	std::unique_ptr<DX12ResourceBuffer> m_shaderTableRayGen;
	std::unique_ptr<DX12ResourceBuffer> m_shaderTableMiss;
	std::unique_ptr<DX12ResourceBuffer> m_shaderTableHit;

	// ray output ping-pong buffer for accumulation.
	std::unique_ptr<DX12ResourceTexture> m_rayOutput; // odd frame: read(SRV), even frame: write(UAV)
	std::unique_ptr<DX12View> m_rayOutputViewWrite;
	std::unique_ptr<DX12View> m_rayOutputViewRead;
	std::unique_ptr<DX12ResourceTexture> m_rayOutput1; // odd frame: write(UAV), even frame: read(SRV)
	std::unique_ptr<DX12View> m_rayOutputViewWrite1;
	std::unique_ptr<DX12View> m_rayOutputViewRead1;

	// ray var buffers
	std::unique_ptr<DX12ResourceBuffer> m_globalIndexBuffer;
	std::unique_ptr<DX12View> m_globalIndexView;
	std::unique_ptr<DX12ResourceBuffer> m_globalPositionBuffer;
	std::unique_ptr<DX12View> m_globalPositionView;
	std::unique_ptr<DX12ResourceBuffer> m_globalNormalBuffer;
	std::unique_ptr<DX12View> m_globalNormalView;
	std::unique_ptr<DX12ResourceBuffer> m_globalTexcoordBuffer;
	std::unique_ptr<DX12View> m_globalTexcoordView;
	std::unique_ptr<DX12ResourceBuffer> m_globalTangentBuffer;
	std::unique_ptr<DX12View> m_globalTangentView;
	std::unique_ptr<DX12ResourceBuffer> m_geoTableBuffer;
	std::unique_ptr<DX12View> m_geoTableView;
};