#include "stdafx.h"
#include "DX12RayTracingManager.h"
#include "D3DUtil.h"

#include <dxcapi.h>
DX12RayTracingManager::DX12RayTracingManager()
{

}

DX12RayTracingManager::~DX12RayTracingManager()
{

}

void DX12RayTracingManager::InitBLAS(ID3D12Device5* device, DX12CommandList* dx12CommandList, const std::vector<std::unique_ptr<DX12RenderGeometry>>& dx12RenderGeometry)
{
	m_BLAS.clear();
	m_BLASScratch.clear();
	m_BLAS.resize(dx12RenderGeometry.size());
	m_BLASScratch.resize(dx12RenderGeometry.size());

	for (size_t i = 0; i < dx12RenderGeometry.size(); ++i)
	{
		auto* geometry = dx12RenderGeometry[i].get();
		DX12ResourceBuffer* vertexBuffer = geometry->GetDX12VertexBuffer();
		DX12ResourceBuffer* indexBuffer = geometry->GetDX12IndexBuffer();
		vertexBuffer->TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);// need NON_PIXEL_SHADER_RESOURCE state for BuildRaytracingAccelerationStructure()
		indexBuffer->TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);// need NON_PIXEL_SHADER_RESOURCE state for BuildRaytracingAccelerationStructure()
		dx12CommandList->RecordResourceStateTransition();

		const UINT64 vbSize = vertexBuffer->GetResource()->GetDesc().Width;
		const UINT64 ibSize = indexBuffer->GetResource()->GetDesc().Width;
		const UINT vtxCount = geometry->GetVertexCount();
		const UINT idxCount = geometry->GetIndexCount();

		D3D12_RAYTRACING_GEOMETRY_DESC geoDesc{};
		geoDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geoDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		geoDesc.Triangles.Transform3x4 = 0;
		geoDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		geoDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geoDesc.Triangles.IndexCount = idxCount;
		geoDesc.Triangles.VertexCount = vtxCount;
		geoDesc.Triangles.IndexBuffer = indexBuffer->GetResource()->GetGPUVirtualAddress();
		geoDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetResource()->GetGPUVirtualAddress();
		geoDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.NumDescs = 1;
		inputs.pGeometryDescs = &geoDesc;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
		device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
		info.ScratchDataSizeInBytes = AlignUp(info.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
		info.ResultDataMaxSizeInBytes = AlignUp(info.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

		m_BLAS[i] = std::make_unique<DX12ResourceBuffer>();
		m_BLAS[i]->CreateDefaultBuffer(device, info.ResultDataMaxSizeInBytes,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

		m_BLASScratch[i] = std::make_unique<DX12ResourceBuffer>();
		m_BLASScratch[i]->CreateDefaultBuffer(device, info.ScratchDataSizeInBytes,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc{};
		desc.Inputs = inputs;
		desc.ScratchAccelerationStructureData = m_BLASScratch[i]->GetResource()->GetGPUVirtualAddress();
		desc.DestAccelerationStructureData = m_BLAS[i]->GetResource()->GetGPUVirtualAddress();

		dx12CommandList->GetCommandList()->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
	}
}

void DX12RayTracingManager::InitTLAS(
	ID3D12Device5* device,
	ID3D12GraphicsCommandList4* commandList,
	const std::vector<std::unique_ptr<DX12RenderGeometry>>& dx12RenderGeometry,
	std::vector<Render::RenderItem>& renderItem)
{
	const UINT instanceCount = UINT(renderItem.size());
	const UINT instBytes = AlignUp(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instanceCount, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT);
	m_instanceDesc = std::make_unique<DX12ResourceBuffer>();
	m_instanceDesc->CreateUploadBuffer(device, instBytes);

	// map geometry pointer to BLAS
	std::unordered_map<const DX12RenderGeometry*, UINT> geo2BLAS;
	geo2BLAS.reserve(dx12RenderGeometry.size());
	for (UINT i = 0; i < dx12RenderGeometry.size(); ++i) geo2BLAS[dx12RenderGeometry[i].get()] = i;

	// 채우기
	D3D12_RAYTRACING_INSTANCE_DESC* inst = nullptr;
	CD3DX12_RANGE rr(0, 0);
	ThrowIfFailed(m_instanceDesc->GetUploadBuffer()->Map(0, &rr, reinterpret_cast<void**>(&inst)));
	for (UINT i = 0; i < instanceCount; ++i)
	{
		auto& ri = renderItem[i];
		const DX12RenderGeometry* g = ri.GetRenderGeometry();
		const UINT blasIdx = geo2BLAS[g];

		XMFLOAT4X4 world = ri.GetObjectConstant().World;
		memcpy(inst[i].Transform, world.m, sizeof(float) * 12);
		inst[i].InstanceID = i;
		inst[i].InstanceContributionToHitGroupIndex = 0;
		inst[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		inst[i].InstanceMask = 0xFF;
		inst[i].AccelerationStructure = m_BLAS[blasIdx]->GetResource()->GetGPUVirtualAddress();
	}
	m_instanceDesc->GetUploadBuffer()->Unmap(0, nullptr);

	// Prebuild
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.NumDescs = instanceCount;
	inputs.InstanceDescs = m_instanceDesc->GetUploadBuffer()->GetGPUVirtualAddress();
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
	device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
	info.ScratchDataSizeInBytes = AlignUp(info.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	info.ResultDataMaxSizeInBytes = AlignUp(info.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

	m_TLASScratch = std::make_unique<DX12ResourceBuffer>();
	m_TLAS = std::make_unique<DX12ResourceBuffer>();

	m_TLASScratch->CreateDefaultBuffer(device, info.ScratchDataSizeInBytes,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_TLAS->CreateDefaultBuffer(device, info.ResultDataMaxSizeInBytes,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc{};
	desc.Inputs = inputs;
	desc.ScratchAccelerationStructureData = m_TLASScratch->GetResource()->GetGPUVirtualAddress();
	desc.DestAccelerationStructureData = m_TLAS->GetResource()->GetGPUVirtualAddress();

	commandList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
}

void DX12RayTracingManager::InitRayTracingPipeline(ID3D12Device5* device, ID3D12RootSignature* globalRootSignature)
{
	ComPtr<IDxcUtils> utils;    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
	ComPtr<IDxcCompiler3> comp; DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&comp));
	ComPtr<IDxcIncludeHandler> inc; utils->CreateDefaultIncludeHandler(&inc);

	ComPtr<IDxcBlobEncoding> src; utils->LoadFile(L"D:\\MiniEngine\\gltfRenderer\\Shaders\\RayTracing.hlsl", nullptr, &src);
	DxcBuffer buf{ src->GetBufferPointer(), src->GetBufferSize(), DXC_CP_UTF8 };

	std::vector<LPCWSTR> common =
	{
	  L"-Zi", L"-Qembed_debug",
	  L"-I", LR"(D:\MiniEngine\gltfRenderer\Shaders)",
	  L"-I", L"shaders",
	  L"-D", L"NUM_TEXTURE=72",
	};

	std::vector<D3D12_DXIL_LIBRARY_DESC> libs; libs.reserve(5);
	std::vector<D3D12_EXPORT_DESC> exps; exps.reserve(5);
	std::vector<D3D12_STATE_SUBOBJECT> subs; subs.reserve(8);

	ComPtr<IDxcBlob> dxilLib;
	{
		ComPtr<IDxcBlobEncoding> src;
		ThrowIfFailed(utils->LoadFile(L"D:\\MiniEngine\\gltfRenderer\\Shaders\\RayTracing.hlsl", nullptr, &src));

		DxcBuffer buf{ src->GetBufferPointer(), src->GetBufferSize(), DXC_CP_UTF8 };
		LPCWSTR argsLib[] =
		{
			L"-T", L"lib_6_5",
			L"-Zi", L"-Qembed_debug",
			L"-I", LR"(D:\MiniEngine\gltfRenderer\Shaders)",
			L"-I", L"shaders",
			L"-D", L"NUM_TEXTURE=72",
		};

		ComPtr<IDxcResult> res;
		ThrowIfFailed(comp->Compile(&buf, argsLib, _countof(argsLib), inc.Get(), IID_PPV_ARGS(&res)));
		ComPtr<IDxcBlobUtf8> errs;
		res->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errs), nullptr);
		if (errs && errs->GetStringLength() > 0) OutputDebugStringA(errs->GetStringPointer());

		HRESULT st = S_OK;
		ThrowIfFailed(res->GetStatus(&st));
		ThrowIfFailed(res->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxilLib), nullptr));
		assert(dxilLib && dxilLib->GetBufferSize() > 0); //shader error log
	}

	exps.push_back({ L"RayGen" , nullptr, D3D12_EXPORT_FLAG_NONE });
	exps.push_back({ L"Miss" , nullptr, D3D12_EXPORT_FLAG_NONE });
	exps.push_back({ L"ClosestHit" , nullptr, D3D12_EXPORT_FLAG_NONE });
	exps.push_back({ L"ShadowAnyHit" , nullptr, D3D12_EXPORT_FLAG_NONE });
	exps.push_back({ L"ShadowMiss" , nullptr, D3D12_EXPORT_FLAG_NONE });

	D3D12_DXIL_LIBRARY_DESC lib{};
	lib.DXILLibrary = { dxilLib->GetBufferPointer(), dxilLib->GetBufferSize() };
	lib.NumExports = (UINT)exps.size();
	lib.pExports = exps.data();

	D3D12_STATE_SUBOBJECT soLib{ D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &lib };
	subs.push_back(soLib);

	D3D12_HIT_GROUP_DESC hg0{};
	hg0.HitGroupExport = L"HitGroup";
	hg0.ClosestHitShaderImport = L"ClosestHit";
	hg0.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

	D3D12_HIT_GROUP_DESC hg1{};
	hg1.HitGroupExport = L"ShadowHitGroup";
	hg1.AnyHitShaderImport = L"ShadowAnyHit";
	hg1.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

	subs.push_back({ D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hg0 });
	subs.push_back({ D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hg1 });

	ID3D12RootSignature* grs = globalRootSignature;
	subs.push_back({ D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &grs });

	// 셰이더 설정
	D3D12_RAYTRACING_SHADER_CONFIG sc{};
	sc.MaxPayloadSizeInBytes = 16;   // float3 radiance + padding
	sc.MaxAttributeSizeInBytes = 8;  // float2 bary
	subs.push_back({ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &sc });

	// 파이프라인 설정
	D3D12_RAYTRACING_PIPELINE_CONFIG pc{};
	pc.MaxTraceRecursionDepth = 2;
	subs.push_back({ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pc });

	// 생성
	D3D12_STATE_OBJECT_DESC sod{};
	sod.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	sod.NumSubobjects = subs.size();
	sod.pSubobjects = subs.data();
	ThrowIfFailed(device->CreateStateObject(&sod, IID_PPV_ARGS(&m_rtState)));
	ThrowIfFailed(m_rtState.As(&m_rtProps));
}

void DX12RayTracingManager::CreateShaderTable(ID3D12Device5* device)
{
	const UINT sid = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	const UINT align = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;

	// RayGen(1)
	{
		const UINT rec = AlignUp(sid, align);
		m_sbtRayGen = std::make_unique<DX12ResourceBuffer>();
		m_sbtRayGen->CreateConstantBuffer(device, rec);
		uint8_t* p = nullptr; CD3DX12_RANGE rr(0, 0);
		ThrowIfFailed(m_sbtRayGen->GetResource()->Map(0, &rr, reinterpret_cast<void**>(&p)));
		memset(p, 0, rec);
		memcpy(p, m_rtProps->GetShaderIdentifier(L"RayGen"), sid);
		m_sbtRayGen->GetResource()->Unmap(0, nullptr);
	}

	// Miss(2): [0]=Miss, [1]=ShadowMiss
	{
		const UINT stride = AlignUp(sid, align);
		const UINT total = stride * 2;
		m_sbtMiss = std::make_unique<DX12ResourceBuffer>();
		m_sbtMiss->CreateConstantBuffer(device, total);
		uint8_t* p = nullptr; CD3DX12_RANGE rr(0, 0);
		ThrowIfFailed(m_sbtMiss->GetResource()->Map(0, &rr, reinterpret_cast<void**>(&p)));
		memset(p, 0, total);
		memcpy(p + 0 * stride, m_rtProps->GetShaderIdentifier(L"Miss"), sid);
		memcpy(p + 1 * stride, m_rtProps->GetShaderIdentifier(L"ShadowMiss"), sid);
		m_sbtMiss->GetResource()->Unmap(0, nullptr);
	}

	// Hit(2): [0]=HitGroup, [1]=ShadowHitGroup
	{
		const UINT stride = AlignUp(sid, align);
		const UINT total = stride * 2;
		m_sbtHit = std::make_unique<DX12ResourceBuffer>();
		m_sbtHit->CreateConstantBuffer(device, total);
		uint8_t* p = nullptr; CD3DX12_RANGE rr(0, 0);
		ThrowIfFailed(m_sbtHit->GetResource()->Map(0, &rr, reinterpret_cast<void**>(&p)));
		memset(p, 0, total);
		memcpy(p + 0 * stride, m_rtProps->GetShaderIdentifier(L"HitGroup"), sid);
		memcpy(p + 1 * stride, m_rtProps->GetShaderIdentifier(L"ShadowHitGroup"), sid);
		m_sbtHit->GetResource()->Unmap(0, nullptr);
	}
}

void DX12RayTracingManager::InitRayOut(ID3D12Device5* device, DX12CommandList* dx12CommandList, DX12DescriptorHeap* dx12DescriptorHeap, UINT width, UINT height)
{
	//출력 UAV 텍스처
	m_rayOutput = std::make_unique<DX12ResourceTexture>();
	m_rayOutput->CreateUAVTexture(
		device,
		width,
		height,
		DXGI_FORMAT_R8G8B8A8_UNORM);

	auto uavCPU = dx12DescriptorHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + 2 * EngineConfig::MaxTextureCount + 5).cpuDescHandle;
	auto uavGPU = dx12DescriptorHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + 2 * EngineConfig::MaxTextureCount + 5).gpuDescHandle;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	m_rayOutputView = std::make_unique<DX12View>(
		device,
		EViewType::EUnorderedAccessView,
		m_rayOutput.get(),
		uavCPU,
		nullptr,
		nullptr,
		&uavDesc);
	m_rayOutput->TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void DX12RayTracingManager::BuildRayGeometryBuffers(
	ID3D12Device5* device,
	DX12CommandList* dx12CommandList,
	DX12DescriptorHeap* dx12DescriptorHeap,
	const std::vector<uint32_t>& indices,
	const std::vector<DirectX::XMFLOAT3>& positions,
	const std::vector<DirectX::XMFLOAT3>& normals,
	const std::vector<DirectX::XMFLOAT2>& texcoords,
	const std::vector<DirectX::XMFLOAT4>& tangents,
	const std::vector<GeometryMetadataCPU>& geoTable)
{
	// 인덱스
	if (!indices.empty())
	{
		m_globalIndexBuffer = std::make_unique<DX12ResourceBuffer>();
		m_globalIndexBuffer->CreateResourceBuffer(
			device,
			dx12CommandList,
			indices.data(),
			UINT64(indices.size() * sizeof(uint32_t)),
			D3D12_RESOURCE_FLAG_NONE);
	}
	else
	{
		m_globalIndexBuffer.reset();
	}

	// 포지션 float3
	if (!positions.empty())
	{
		m_globalPositionBuffer = std::make_unique<DX12ResourceBuffer>();
		m_globalPositionBuffer->CreateResourceBuffer(
			device,
			dx12CommandList,
			positions.data(),
			UINT64(positions.size() * sizeof(DirectX::XMFLOAT3)),
			D3D12_RESOURCE_FLAG_NONE);
	}
	else
	{
		m_globalPositionBuffer.reset();
	}

	// 노멀 float3
	if (!normals.empty())
	{
		m_globalNormalBuffer = std::make_unique<DX12ResourceBuffer>();
		m_globalNormalBuffer->CreateResourceBuffer(
			device,
			dx12CommandList,
			normals.data(),
			UINT64(normals.size() * sizeof(DirectX::XMFLOAT3)),
			D3D12_RESOURCE_FLAG_NONE);
	}
	else
	{
		m_globalNormalBuffer.reset();
	}

	// 텍스쳐 좌표 float2
	if (!texcoords.empty())
	{
		m_globalTexcoordBuffer = std::make_unique<DX12ResourceBuffer>();
		m_globalTexcoordBuffer->CreateResourceBuffer(
			device,
			dx12CommandList,
			texcoords.data(),
			UINT64(texcoords.size() * sizeof(DirectX::XMFLOAT2)),
			D3D12_RESOURCE_FLAG_NONE);
	}
	else
	{
		m_globalTexcoordBuffer.reset();
	}

	// 탄젠트 float4
	if (!tangents.empty())
	{
		m_globalTangentBuffer = std::make_unique<DX12ResourceBuffer>();
		m_globalTangentBuffer->CreateResourceBuffer(
			device,
			dx12CommandList,
			tangents.data(),
			UINT64(tangents.size() * sizeof(DirectX::XMFLOAT4)),
			D3D12_RESOURCE_FLAG_NONE);
	}
	else
	{
		m_globalTangentBuffer.reset();
	}

	// 기하 테이블 16바이트
	if (!geoTable.empty())
	{
		m_geoTableBuffer = std::make_unique<DX12ResourceBuffer>();
		m_geoTableBuffer->CreateResourceBuffer(
			device,
			dx12CommandList,
			geoTable.data(),
			UINT64(geoTable.size() * sizeof(GeometryMetadataCPU)),
			D3D12_RESOURCE_FLAG_NONE);
	}
	else
	{
		m_geoTableBuffer.reset();
	}

	WriteSceneBufferSRV(device, dx12DescriptorHeap);
}

void DX12RayTracingManager::WriteSceneBufferSRV(ID3D12Device5* device, DX12DescriptorHeap* dx12DescriptorHeap)
{
	auto MakeBufSRV = [&](DX12ResourceBuffer* res, DXGI_FORMAT fmt, UINT firstElem, UINT numElem, UINT stride) {
		D3D12_SHADER_RESOURCE_VIEW_DESC s{};
		s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		s.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		s.Format = fmt;
		s.Buffer.FirstElement = firstElem;
		s.Buffer.NumElements = numElem;
		s.Buffer.StructureByteStride = stride;
		s.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		return s;
		};
	auto NumElems = [&](ID3D12Resource* res, UINT bytesPerElem) {
		return (UINT)(res->GetDesc().Width / bytesPerElem);
		};

	// TLAS
	{
		auto cpu = dx12DescriptorHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + 2 * EngineConfig::MaxTextureCount + 3).cpuDescHandle;
		D3D12_SHADER_RESOURCE_VIEW_DESC s{};
		s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		s.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		s.RaytracingAccelerationStructure.Location = m_TLAS->GetResource()->GetGPUVirtualAddress();

		m_TLASView = std::make_unique<DX12View>(
			device,
			EViewType::EShaderResourceView,
			nullptr,
			cpu,
			nullptr,
			&s);
	}

	// geometry
	{
		const UINT stride = 16;
		const UINT n = NumElems(m_geoTableBuffer->GetResource(), stride);
		auto cpu = dx12DescriptorHeap->Offset(EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + 2 * EngineConfig::MaxTextureCount + 4).cpuDescHandle;
		auto s = MakeBufSRV(m_geoTableBuffer.get(), DXGI_FORMAT_UNKNOWN, 0, n, stride);
		m_geoTableView = std::make_unique<DX12View>(
			device,
			EViewType::EShaderResourceView,
			m_geoTableBuffer.get(),
			cpu,
			nullptr,
			&s);
	}
	
	{
		UINT slot = EngineConfig::ConstantBufferCount * EngineConfig::SwapChainBufferCount + 2 * EngineConfig::MaxTextureCount + 6; // srv 인덱스 전역화/통일 필요. 너무 복잡해짐

		// index
		{
			const UINT n = NumElems(m_globalIndexBuffer->GetResource(), 4);
			auto cpu = dx12DescriptorHeap->Offset(slot + 0).cpuDescHandle;
			auto s = MakeBufSRV(m_globalIndexBuffer.get(), DXGI_FORMAT_R32_UINT, 0, n, 0);
			m_globalIndexView = std::make_unique<DX12View>(
				device,
				EViewType::EShaderResourceView,
				m_globalIndexBuffer.get(),
				cpu,
				nullptr,
				&s);
		}
		// positino
		{
			const UINT n = NumElems(m_globalPositionBuffer->GetResource(), 4);
			auto cpu = dx12DescriptorHeap->Offset(slot + 1).cpuDescHandle;
			auto s = MakeBufSRV(m_globalPositionBuffer.get(), DXGI_FORMAT_R32_UINT, 0, n, 0);
			m_globalPositionView = std::make_unique<DX12View>(
				device,
				EViewType::EShaderResourceView,
				m_globalPositionBuffer.get(),
				cpu,
				nullptr,
				&s);
		}
		// normal
		{
			const UINT n = NumElems(m_globalNormalBuffer->GetResource(), 4);
			auto cpu = dx12DescriptorHeap->Offset(slot + 2).cpuDescHandle;
			auto s = MakeBufSRV(m_globalNormalBuffer.get(), DXGI_FORMAT_R32_UINT, 0, n, 0);
			m_globalNormalView = std::make_unique<DX12View>(
				device,
				EViewType::EShaderResourceView,
				m_globalNormalBuffer.get(),
				cpu,
				nullptr,
				&s);
		}
		// tex cord
		{
			const UINT n = NumElems(m_globalTexcoordBuffer->GetResource(), 4);
			auto cpu = dx12DescriptorHeap->Offset(slot + 3).cpuDescHandle;
			auto s = MakeBufSRV(m_globalTexcoordBuffer.get(), DXGI_FORMAT_R32_UINT, 0, n, 0);
			m_globalTexcoordView = std::make_unique<DX12View>(
				device,
				EViewType::EShaderResourceView,
				m_globalTexcoordBuffer.get(),
				cpu,
				nullptr,
				&s);
		}
		// tangent
		{
			const UINT n = NumElems(m_globalTangentBuffer->GetResource(), 4);
			auto cpu = dx12DescriptorHeap->Offset(slot + 4).cpuDescHandle;
			auto s = MakeBufSRV(m_globalTangentBuffer.get(), DXGI_FORMAT_R32_UINT, 0, n, 0);
			m_globalTangentView = std::make_unique<DX12View>(
				device,
				EViewType::EShaderResourceView,
				m_globalTangentBuffer.get(),
				cpu,
				nullptr,
				&s);
		}
	}
}