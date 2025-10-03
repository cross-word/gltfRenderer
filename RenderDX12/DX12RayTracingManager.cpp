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

		const UINT vtxCount = geometry->GetVertexCount();
		const UINT idxCount = geometry->GetIndexCount();

		D3D12_RAYTRACING_GEOMETRY_DESC geoDesc{};
		geoDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geoDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
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
		m_BLAS[i]->CreateResource(
			device,
			info.ResultDataMaxSizeInBytes,
			D3D12_HEAP_TYPE_DEFAULT,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

		m_BLASScratch[i] = std::make_unique<DX12ResourceBuffer>();
		m_BLASScratch[i]->CreateResource(
			device, 
			info.ScratchDataSizeInBytes,
			D3D12_HEAP_TYPE_DEFAULT,
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
	const UINT64 instBytes = AlignUp(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instanceCount, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT);
	m_instanceDesc = std::make_unique<DX12ResourceBuffer>();
	m_instanceDesc->CreateUploadBuffer(device, instBytes);

	// map geometry pointer to BLAS
	std::unordered_map<const DX12RenderGeometry*, UINT> geo2BLAS;
	geo2BLAS.reserve(dx12RenderGeometry.size());
	for (UINT i = 0; i < dx12RenderGeometry.size(); ++i) geo2BLAS[dx12RenderGeometry[i].get()] = i;

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

	m_TLASScratch->CreateResource(
		device,
		info.ScratchDataSizeInBytes,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_TLAS->CreateResource(
		device,
		info.ResultDataMaxSizeInBytes,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc{};
	desc.Inputs = inputs;
	desc.ScratchAccelerationStructureData = m_TLASScratch->GetResource()->GetGPUVirtualAddress();
	desc.DestAccelerationStructureData = m_TLAS->GetResource()->GetGPUVirtualAddress();

	commandList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
}

void DX12RayTracingManager::InitRayTracingPipeline(ID3D12Device5* device, ID3D12RootSignature* globalRootSignature, std::vector<std::string>& shaderMacros)
{
	ComPtr<IDxcUtils> utils;
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
	ComPtr<IDxcCompiler3> compiler;
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
	ComPtr<IDxcIncludeHandler> includeHandler;
	utils->CreateDefaultIncludeHandler(&includeHandler);

	std::vector<D3D12_DXIL_LIBRARY_DESC> libs;
	libs.reserve(5);
	std::vector<D3D12_EXPORT_DESC> exps;
	exps.reserve(5);
	std::vector<D3D12_STATE_SUBOBJECT> subs;
	subs.reserve(8);

	ComPtr<IDxcBlob> dxilLib;
	{
		ComPtr<IDxcBlobEncoding> src;
		ThrowIfFailed(utils->LoadFile(EngineConfig::ShaderRayTracingPath, nullptr, &src));

		DxcBuffer buf{ src->GetBufferPointer(), src->GetBufferSize(), DXC_CP_UTF8 };

		std::wstring numTex = L"NUM_TEXTURE=" + AnsiToWString(shaderMacros[0]);
		std::wstring numLight = L"NUM_LIGHTS=" + AnsiToWString(shaderMacros[1]);
		std::wstring numDirLight = L"NUM_DIR_LIGHTS=" + AnsiToWString(shaderMacros[2]);
		std::wstring numPointLight = L"NUM_POINT_LIGHTS=" + AnsiToWString(shaderMacros[3]);
		std::wstring numSpotLight = L"NUM_SPOT_LIGHTS=" + AnsiToWString(shaderMacros[4]);

		LPCWSTR argsLib[] =
		{
			L"-T", L"lib_6_5",
			L"-Zi", L"-Qembed_debug",
			L"-I", EngineConfig::ShaderDirectoryPath,
			L"-I", L"shaders",
			L"-D", numTex.c_str(),
			L"-D", numLight.c_str(),
			L"-D", numDirLight.c_str(),
			L"-D", numPointLight.c_str(),
			L"-D", numSpotLight.c_str(),
		};

		ComPtr<IDxcResult> result;
		ThrowIfFailed(compiler->Compile(&buf, argsLib, _countof(argsLib), includeHandler.Get(), IID_PPV_ARGS(&result)));
		ComPtr<IDxcBlobUtf8> errs;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errs), nullptr);
		if (errs && errs->GetStringLength() > 0) OutputDebugStringA(errs->GetStringPointer());

		HRESULT st = S_OK;
		ThrowIfFailed(result->GetStatus(&st));
		ThrowIfFailed(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxilLib), nullptr));
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

	ID3D12RootSignature* globalRS = globalRootSignature;
	subs.push_back({ D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &globalRS });

	// shader config
	D3D12_RAYTRACING_SHADER_CONFIG shaderConfig{};
	shaderConfig.MaxPayloadSizeInBytes = 16;   // float3 radiance + padding
	shaderConfig.MaxAttributeSizeInBytes = 8;  // float2 bary
	subs.push_back({ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shaderConfig });

	// pipeline config
	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig{};
	pipelineConfig.MaxTraceRecursionDepth = 2;
	subs.push_back({ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipelineConfig });

	// state object generate
	D3D12_STATE_OBJECT_DESC stateObjectDesc{};
	stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
	stateObjectDesc.NumSubobjects = SizeToU32(subs.size());
	stateObjectDesc.pSubobjects = subs.data();
	ThrowIfFailed(device->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(&m_RTState)));
	ThrowIfFailed(m_RTState.As(&m_RTProps));
}

void DX12RayTracingManager::CreateShaderTable(ID3D12Device5* device)
{
	const UINT shaderIdentifier = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	const UINT align = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;

	{// RayGen
		const UINT64 record = AlignUp(shaderIdentifier, align);
		m_shaderTableRayGen = std::make_unique<DX12ResourceBuffer>();
		m_shaderTableRayGen->CreateResource(
			device,
			record,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_RESOURCE_STATE_GENERIC_READ);
		uint8_t* mapped = nullptr;
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_shaderTableRayGen->GetResource()->Map(0, &readRange, reinterpret_cast<void**>(&mapped)));
		memset(mapped, 0, record);
		memcpy(mapped, m_RTProps->GetShaderIdentifier(L"RayGen"), shaderIdentifier);
		m_shaderTableRayGen->GetResource()->Unmap(0, nullptr);
	}

	{// Miss : Miss, ShadowMiss
		const UINT64 stride = AlignUp(shaderIdentifier, align);
		const UINT64 totalRecord = stride * 2;
		m_shaderTableMiss = std::make_unique<DX12ResourceBuffer>();
		m_shaderTableMiss->CreateResource(
			device,
			totalRecord,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_RESOURCE_STATE_GENERIC_READ);
		uint8_t* mapped = nullptr;
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_shaderTableMiss->GetResource()->Map(0, &readRange, reinterpret_cast<void**>(&mapped)));
		memset(mapped, 0, totalRecord);
		memcpy(mapped + 0 * stride, m_RTProps->GetShaderIdentifier(L"Miss"), shaderIdentifier);
		memcpy(mapped + 1 * stride, m_RTProps->GetShaderIdentifier(L"ShadowMiss"), shaderIdentifier);
		m_shaderTableMiss->GetResource()->Unmap(0, nullptr);
	}

	{// Hit : HitGroup, ShadowHitGroup
		const UINT64 stride = AlignUp(shaderIdentifier, align);
		const UINT64 totalRecord = stride * 2;
		m_shaderTableHit = std::make_unique<DX12ResourceBuffer>();
		m_shaderTableHit->CreateResource(
			device,
			totalRecord,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_RESOURCE_STATE_GENERIC_READ);
		uint8_t* mapped = nullptr;
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_shaderTableHit->GetResource()->Map(0, &readRange, reinterpret_cast<void**>(&mapped)));
		memset(mapped, 0, totalRecord);
		memcpy(mapped + 0 * stride, m_RTProps->GetShaderIdentifier(L"HitGroup"), shaderIdentifier);
		memcpy(mapped + 1 * stride, m_RTProps->GetShaderIdentifier(L"ShadowHitGroup"), shaderIdentifier);
		m_shaderTableHit->GetResource()->Unmap(0, nullptr);
	}
}

void DX12RayTracingManager::InitRayOut(ID3D12Device5* device, DX12CommandList* dx12CommandList, DX12DescriptorHeap* dx12DescriptorHeap, UINT width, UINT height)
{
	// ray output texture (UAV)
	m_rayOutput = std::make_unique<DX12ResourceTexture>();
	m_rayOutput->CreateUAVTexture(
		device,
		width,
		height,
		DXGI_FORMAT_R8G8B8A8_UNORM);

	auto uavCPU = dx12DescriptorHeap->Offset(SRVOffset::SRVOffsetRayOutput).cpuDescHandle;
	auto uavGPU = dx12DescriptorHeap->Offset(SRVOffset::SRVOffsetRayOutput).gpuDescHandle;

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
	const std::vector<XMFLOAT3>& positions,
	const std::vector<XMFLOAT3>& normals,
	const std::vector<XMFLOAT2>& texcoords,
	const std::vector<XMFLOAT4>& tangents,
	const std::vector<GeometryMetadataCPU>& geoTable)
{
	// index
	if (!indices.empty())
	{
		m_globalIndexBuffer = std::make_unique<DX12ResourceBuffer>();
		m_globalIndexBuffer->CreateResourceAndUploadBuffer(
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

	// position float3
	if (!positions.empty())
	{
		m_globalPositionBuffer = std::make_unique<DX12ResourceBuffer>();
		m_globalPositionBuffer->CreateResourceAndUploadBuffer(
			device,
			dx12CommandList,
			positions.data(),
			UINT64(positions.size() * sizeof(XMFLOAT3)),
			D3D12_RESOURCE_FLAG_NONE);
	}
	else
	{
		m_globalPositionBuffer.reset();
	}

	// normal float3
	if (!normals.empty())
	{
		m_globalNormalBuffer = std::make_unique<DX12ResourceBuffer>();
		m_globalNormalBuffer->CreateResourceAndUploadBuffer(
			device,
			dx12CommandList,
			normals.data(),
			UINT64(normals.size() * sizeof(XMFLOAT3)),
			D3D12_RESOURCE_FLAG_NONE);
	}
	else
	{
		m_globalNormalBuffer.reset();
	}

	// texcoord float2
	if (!texcoords.empty())
	{
		m_globalTexcoordBuffer = std::make_unique<DX12ResourceBuffer>();
		m_globalTexcoordBuffer->CreateResourceAndUploadBuffer(
			device,
			dx12CommandList,
			texcoords.data(),
			UINT64(texcoords.size() * sizeof(XMFLOAT2)),
			D3D12_RESOURCE_FLAG_NONE);
	}
	else
	{
		m_globalTexcoordBuffer.reset();
	}

	// tangent float4
	if (!tangents.empty())
	{
		m_globalTangentBuffer = std::make_unique<DX12ResourceBuffer>();
		m_globalTangentBuffer->CreateResourceAndUploadBuffer(
			device,
			dx12CommandList,
			tangents.data(),
			UINT64(tangents.size() * sizeof(XMFLOAT4)),
			D3D12_RESOURCE_FLAG_NONE);
	}
	else
	{
		m_globalTangentBuffer.reset();
	}

	// geometry 16byte
	if (!geoTable.empty())
	{
		m_geoTableBuffer = std::make_unique<DX12ResourceBuffer>();
		m_geoTableBuffer->CreateResourceAndUploadBuffer(
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
	auto BuildDesc = [&](DXGI_FORMAT format, UINT firstElem, UINT numElem, UINT stride) {
		D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		desc.Format = format;
		desc.Buffer.FirstElement = firstElem;
		desc.Buffer.NumElements = numElem;
		desc.Buffer.StructureByteStride = stride;
		desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		return desc;
		};

	// TLAS
	{
		auto cpuHandle = dx12DescriptorHeap->Offset(SRVOffset::SRVOffsetRayTLAS).cpuDescHandle;
		D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		desc.RaytracingAccelerationStructure.Location = m_TLAS->GetResource()->GetGPUVirtualAddress();

		m_TLASView = std::make_unique<DX12View>(
			device,
			EViewType::EShaderResourceView,
			nullptr,
			cpuHandle,
			nullptr,
			&desc);
	}

	// geometry
	{
		const UINT stride = 16;
		const UINT numElem = (UINT)m_geoTableBuffer->GetResource()->GetDesc().Width / stride;
		auto cpuHandle = dx12DescriptorHeap->Offset(SRVOffset::SRVOffsetRayGeometry).cpuDescHandle;
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = BuildDesc(DXGI_FORMAT_UNKNOWN, 0, numElem, stride);
		m_geoTableView = std::make_unique<DX12View>(
			device,
			EViewType::EShaderResourceView,
			m_geoTableBuffer.get(),
			cpuHandle,
			nullptr,
			&desc);
	}
	// index
	{
		const UINT numElem = (UINT)m_globalIndexBuffer->GetResource()->GetDesc().Width / 4;
		auto cpuHandle = dx12DescriptorHeap->Offset(SRVOffset::SRVOffsetRayIndex).cpuDescHandle;
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = BuildDesc(DXGI_FORMAT_R32_UINT, 0, numElem, 0);
		m_globalIndexView = std::make_unique<DX12View>(
			device,
			EViewType::EShaderResourceView,
			m_globalIndexBuffer.get(),
			cpuHandle,
			nullptr,
			&desc);
	}
	// positino
	{
		const UINT numElem = (UINT)m_globalPositionBuffer->GetResource()->GetDesc().Width / 4;
		auto cpuHandle = dx12DescriptorHeap->Offset(SRVOffset::SRVOffsetRayPosition).cpuDescHandle;
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = BuildDesc(DXGI_FORMAT_R32_UINT, 0, numElem, 0);
		m_globalPositionView = std::make_unique<DX12View>(
			device,
			EViewType::EShaderResourceView,
			m_globalPositionBuffer.get(),
			cpuHandle,
			nullptr,
			&desc);
	}
	// normal
	{
		const UINT numElem = (UINT)m_globalNormalBuffer->GetResource()->GetDesc().Width / 4;
		auto cpuHandle = dx12DescriptorHeap->Offset(SRVOffset::SRVOffsetRayNormal).cpuDescHandle;
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = BuildDesc(DXGI_FORMAT_R32_UINT, 0, numElem, 0);
		m_globalNormalView = std::make_unique<DX12View>(
			device,
			EViewType::EShaderResourceView,
			m_globalNormalBuffer.get(),
			cpuHandle,
			nullptr,
			&desc);
	}
	// tex coord
	{
		const UINT numElem = (UINT)m_globalTexcoordBuffer->GetResource()->GetDesc().Width / 4;
		auto cpuHandle = dx12DescriptorHeap->Offset(SRVOffset::SRVOffsetRayTexCoord).cpuDescHandle;
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = BuildDesc(DXGI_FORMAT_R32_UINT, 0, numElem, 0);
		m_globalTexcoordView = std::make_unique<DX12View>(
			device,
			EViewType::EShaderResourceView,
			m_globalTexcoordBuffer.get(),
			cpuHandle,
			nullptr,
			&desc);
	}
	// tangent
	{
		const UINT numElem = (UINT)m_globalTangentBuffer->GetResource()->GetDesc().Width / 4;
		auto cpuHandle = dx12DescriptorHeap->Offset(SRVOffset::SRVOffsetRayTangent).cpuDescHandle;
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = BuildDesc(DXGI_FORMAT_R32_UINT, 0, numElem, 0);
		m_globalTangentView = std::make_unique<DX12View>(
			device,
			EViewType::EShaderResourceView,
			m_globalTangentBuffer.get(),
			cpuHandle,
			nullptr,
			&desc);
	}
}