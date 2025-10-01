#pragma once

#include "stdafx.h"
#include "DX12Resource.h"
#include "DX12RenderGeometry.h"
#include "DX12DescriptorHeap.h"

struct GeometryMetadataCPU {
	uint32_t IndexOffset;   // 인덱스 시작(글로벌)
	uint32_t VertexOffset;  // 버텍스 시작(글로벌)
	uint32_t MaterialIndex; // 머티리얼 인덱스
	uint32_t _pad;          // 16바이트 정렬
};


class DX12RayTracingManager
{
public:
	DX12RayTracingManager();
	~DX12RayTracingManager();

	ID3D12Resource* GetBLAS(UINT index) const noexcept { return m_BLAS[index]->GetResource(); }
	ID3D12Resource* GetTLAS() const noexcept { return m_TLAS->GetResource(); }
	ID3D12StateObject* GetStateObject() const noexcept { return m_rtState.Get(); }
	ID3D12StateObjectProperties* GetStateObjectProperties() const noexcept { return m_rtProps.Get(); }
	ID3D12Resource* GetRayGenShaderTable() const noexcept { return m_sbtRayGen->GetResource(); }
	ID3D12Resource* GetMissShaderTable() const noexcept { return m_sbtMiss->GetResource(); }
	ID3D12Resource* GetHitShaderTable() const noexcept { return m_sbtHit->GetResource(); }
	DX12ResourceTexture* GetRayOut() const noexcept { return m_rayOutput.get(); }

public:
	void InitBLAS(
		ID3D12Device5* device,
		DX12CommandList* dx12CommandList,
		const std::vector<std::unique_ptr<DX12RenderGeometry>>& dx12RenderGeometry);
	void InitTLAS(
		ID3D12Device5* device,
		ID3D12GraphicsCommandList4* commandList,
		const std::vector<std::unique_ptr<DX12RenderGeometry>>& dx12RenderGeometry,
		std::vector<Render::RenderItem>& renderItem);
	void InitRayTracingPipeline(ID3D12Device5* device, ID3D12RootSignature* globalRootSignature);
	void CreateShaderTable(ID3D12Device5* device);
	void InitRayOut(ID3D12Device5* device, DX12CommandList* dx12CommandList, DX12DescriptorHeap* dx12DescriptorHeap, UINT width, UINT height);
	void BuildRayGeometryBuffers(
		ID3D12Device5* device,
		DX12CommandList* dx12CommandList,
		DX12DescriptorHeap* dx12DescriptorHeap,
		const std::vector<uint32_t>& indices,
		const std::vector<DirectX::XMFLOAT3>& positions,
		const std::vector<DirectX::XMFLOAT3>& normals,
		const std::vector<DirectX::XMFLOAT2>& texcoords,
		const std::vector<DirectX::XMFLOAT4>& tangents,
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

	// 파이프라인
	ComPtr<ID3D12StateObject> m_rtState;
	ComPtr<ID3D12StateObjectProperties> m_rtProps;

	// 셰이더 테이블
	std::unique_ptr<DX12ResourceBuffer> m_sbtRayGen;
	std::unique_ptr<DX12ResourceBuffer> m_sbtMiss;
	std::unique_ptr<DX12ResourceBuffer> m_sbtHit;

	// 레이 결과
	std::unique_ptr<DX12ResourceTexture> m_rayOutput;
	std::unique_ptr<DX12View> m_rayOutputView;

	// ---- 레이용 변수 버퍼들 ----
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
	std::unique_ptr<DX12ResourceBuffer> m_geoTableBuffer; // StructuredBuffer<GeometryMetadata>
	std::unique_ptr<DX12View> m_geoTableView;
};