#include "stdafx.h"
#include "DX12RootSignature.h"
#include "D3DUtil.h"

DX12RootSignature::DX12RootSignature()
{

}

DX12RootSignature::~DX12RootSignature()
{

}

void DX12RootSignature::Initialize(ID3D12Device* device)
{
	CreateRasterizeRootSignature(device);
	CreateRayTracingRootSignature(device);
}

void DX12RootSignature::CreateRasterizeRootSignature(ID3D12Device* device)
{
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER1 slotRootParameter[6];

	// Create a single descriptor table of CBVs.
	CD3DX12_DESCRIPTOR_RANGE1 cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	// Create descriptor table of texture.
	CD3DX12_DESCRIPTOR_RANGE1 srvTexTable[2];
	srvTexTable[0].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		EngineConfig::MaxTextureCount,
		0,
		0);//t0 space 0 SRGB texture
	srvTexTable[1].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		EngineConfig::MaxTextureCount,
		0,
		2);//t0 space 2 Linear texture

	// Create descriptor table of material/world vectors.
	CD3DX12_DESCRIPTOR_RANGE1 srvTable[2];
	srvTable[0].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,
		0,
		1);//t0 space 1 materials
	srvTable[1].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,
		1,
		1);//t1 space 1 objectConstant

	// Create a single descriptor table of shadow map.
	CD3DX12_DESCRIPTOR_RANGE1 shadowMapTable;
	shadowMapTable.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,
		2,
		1); //t2 space1 shadow

	CD3DX12_DESCRIPTOR_RANGE1 iblTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 3, 1); //t3 space1

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);
	slotRootParameter[1].InitAsDescriptorTable(2, srvTexTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[2].InitAsDescriptorTable(2, srvTable, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[3].InitAsDescriptorTable(1, &shadowMapTable, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[4].InitAsConstants(3, 2, 0); // index of textur/material/world
	slotRootParameter[5].InitAsDescriptorTable(1, &iblTable, D3D12_SHADER_VISIBILITY_PIXEL);

	const UINT samplerSize = SizeToU32(staticSamplers.size());
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(6, slotRootParameter, samplerSize, staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr;
	hr = D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &errorBlob);
	if (FAILED(hr))
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		errorBlob->Release();
	}
	ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rasterizeRootSignature)));

	return;
}

void DX12RootSignature::CreateRayTracingRootSignature(ID3D12Device* device)
{
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER1 slotRootParameter[11];

	// Create a single descriptor table of CBVs.
	CD3DX12_DESCRIPTOR_RANGE1 cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	// Create descriptor table of texture.
	CD3DX12_DESCRIPTOR_RANGE1 srvTexTable[2];
	srvTexTable[0].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		EngineConfig::MaxTextureCount,
		0,
		0);//t0 space 0 SRGB texture
	srvTexTable[1].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		EngineConfig::MaxTextureCount,
		0,
		2);//t0 space 2 Linear texture

	// Create descriptor table of material/world vectors.
	CD3DX12_DESCRIPTOR_RANGE1 srvTable[2];
	srvTable[0].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,
		0,
		1);//t0 space 1 materials
	srvTable[1].Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,
		1,
		1);//t1 space 1 objectConstant
	CD3DX12_DESCRIPTOR_RANGE1 srvTableGeo;
	srvTableGeo.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,
		6,
		1);//t4 space1 geometry table

	// Create a single descriptor table of shadow map.
	CD3DX12_DESCRIPTOR_RANGE1 shadowMapTable;
	shadowMapTable.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,
		2,
		1); //t2 space1 shadow

	//t0 space4
	CD3DX12_DESCRIPTOR_RANGE1 TLASTable; 
	TLASTable.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,
		0,
		4); //t0 space4 TLAS

	//u0, u1 space0
	CD3DX12_DESCRIPTOR_RANGE1 RayOutTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0); //u0 space0 RayOut
	CD3DX12_DESCRIPTOR_RANGE1 RayOutPrevTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 4); //t1 space4 accumulate
	//CD3DX12_DESCRIPTOR_RANGE1 RayOutCurrentTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 4); //t2 space4 accumulate

	//t0~4 space3
	CD3DX12_DESCRIPTOR_RANGE1 rayVar(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0, 3);

	CD3DX12_DESCRIPTOR_RANGE1 iblTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 3, 1); //t3 space1

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);
	slotRootParameter[1].InitAsDescriptorTable(2, srvTexTable, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[2].InitAsDescriptorTable(2, srvTable, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[3].InitAsDescriptorTable(1, &shadowMapTable, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[4].InitAsConstants(3, 2, 0); // index of texture/material/world
	slotRootParameter[5].InitAsDescriptorTable(1, &iblTable, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[6].InitAsDescriptorTable(1, &TLASTable, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[7].InitAsDescriptorTable(1, &srvTableGeo, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[8].InitAsDescriptorTable(1, &RayOutTable, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[9].InitAsDescriptorTable(1, &RayOutPrevTable, D3D12_SHADER_VISIBILITY_ALL);
	//slotRootParameter[9].InitAsDescriptorTable(1, &RayOutCurrentTable, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[10].InitAsDescriptorTable(1, &rayVar, D3D12_SHADER_VISIBILITY_ALL);

	const UINT samplerSize = SizeToU32(staticSamplers.size());
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(11, slotRootParameter, samplerSize, staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr;
	hr = D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &errorBlob);
	if (FAILED(hr))
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		errorBlob->Release();
	}
	ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rayTracingRootSignature)));

	return;
}

//instance function of SampleDesc from Frank Luna
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> DX12RootSignature::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC shadowCmp(
		6, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		0.0f,  // mipLODBias
		0,     // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
		0.0f, D3D12_FLOAT32_MAX,
		D3D12_SHADER_VISIBILITY_PIXEL);

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp,
		shadowCmp };
}