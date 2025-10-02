#include "stdafx.h"
#include "DX12FrameResource.h"
#include "DX12ShadowManager.h"

DX12FrameResource::DX12FrameResource(ID3D12Device* device)
{
	//CREATE CMD ALLOC
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_commandAllocator.GetAddressOf())
	));

	//CREATE CMD ALLOC FOR WORKERS
	m_workerAlloc.reserve(EngineConfig::NumThreadWorker);
	for (int i = 0; i < m_workerAlloc.size(); i++)
	{
		ComPtr<ID3D12CommandAllocator> tmpAllocator;
		ThrowIfFailed(device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(tmpAllocator.GetAddressOf())
		));
		m_workerAlloc.push_back(tmpAllocator);
	}
}

DX12FrameResource::~DX12FrameResource()
{

}

void DX12FrameResource::CreateSRV(ID3D12Device* device, DX12DescriptorHeap* dx12DescriptorHeap, uint32_t frameIndex)
{
	//SET HEAP VAR
	//NEED FrameIndex, ThreadIndex, public descNum per frame, private descNum per thread, maxworker // in multi-thread env

	//CREATE CONSTANT BUFFER
	//ALLOCATE GPU ADDRESS
	uint32_t byteSize = CalcConstantBufferByteSize(sizeof(PassConstants));
	auto descCPUAddress = dx12DescriptorHeap->Offset(dx12DescriptorHeap->CalcHeapSliceShareBlock(frameIndex, 0, EngineConfig::ConstantBufferCount, 0, 0)).cpuDescHandle;

	//PassConstantBuffer
	m_DX12PassConstantBuffer = std::make_unique<DX12ResourceBuffer>();
	m_DX12PassConstantBuffer->CreateConstantBuffer(device, byteSize);
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_DX12PassConstantBuffer->GetResource()->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC passCBVDesc;
	passCBVDesc.BufferLocation = cbAddress;
	passCBVDesc.SizeInBytes = byteSize;

	m_DX12PassConstantBufferView = std::make_unique<DX12View>(
		device,
		EViewType::EConstantBufferView,
		m_DX12PassConstantBuffer.get(),
		descCPUAddress,
		&passCBVDesc);
}

void DX12FrameResource::UploadPassConstant(D3DCamera* d3dCamera, std::vector<Light>& lights, D3DTimer d3dTimer)
{
	PassConstants passConst;
	passConst.AmbientLight = { 0.08f, 0.08f, 0.08f, 1.0f };
	Light sun{}; //sun light

	float pi = 3.1415926535f;
	float theta = (2 * pi) / (12000.0f) * d3dTimer.GetTotalTime(); // 12s is one period

	sun.Type = LIGHT_TYPE_DIRECTIONAL;
	sun.Color = { 1.0f, 1.0f, 1.0f };
	sun.Intensity = 1.0f;
	sun.Direction = { 0.0f, -cosf(theta), sinf(theta) };
	sun.Range = -1.0f;
	sun.Position = { 0.0f, 0.0f, 0.0f };
	sun.InnerCos = 0.0f;
	sun.OuterCos = -1.0f;
	passConst.Lights[0] = sun;

	//gather lights from .gltf
	for (uint16_t i = 0; i < lights.size(); ++i)
	{
		passConst.Lights[i + 1].Color = lights[i].Color;
		passConst.Lights[i + 1].Direction = lights[i].Direction;
		passConst.Lights[i + 1].InnerCos = lights[i].InnerCos;
		passConst.Lights[i + 1].Intensity = lights[i].Intensity;
		passConst.Lights[i + 1].OuterCos = lights[i].OuterCos;
		passConst.Lights[i + 1].Position = lights[i].Position;
		passConst.Lights[i + 1].Range = lights[i].Range;
		passConst.Lights[i + 1].Type = lights[i].Type;
	}

	XMMATRIX V = d3dCamera->GetViewMatrix();
	XMMATRIX P = d3dCamera->GetProjectionMatrix(float(EngineConfig::DefaultWidth) / float(EngineConfig::DefaultHeight));
	XMMATRIX VP = XMMatrixMultiply(V, P); // HLSL column-major
	XMMATRIX iV = XMMatrixInverse(nullptr, V);
	XMMATRIX iP = XMMatrixInverse(nullptr, P);
	XMMATRIX iVP = XMMatrixInverse(nullptr, VP);

	XMStoreFloat4x4(&passConst.View, XMMatrixTranspose(V));
	XMStoreFloat4x4(&passConst.Proj, XMMatrixTranspose(P));
	XMStoreFloat4x4(&passConst.ViewProj, XMMatrixTranspose(VP));
	XMStoreFloat4x4(&passConst.InvView, XMMatrixTranspose(iV));
	XMStoreFloat3(&passConst.EyePosW, iV.r[3]);
	XMStoreFloat4x4(&passConst.InvProj, XMMatrixTranspose(iP));
	XMStoreFloat4x4(&passConst.InvViewProj, XMMatrixTranspose(iVP));

	XMVECTOR lightDirWS = XMVector3Normalize(XMLoadFloat3(&sun.Direction));
	XMMATRIX lightView, lightProj;
	BuildDirLightViewProj(lightDirWS, iVP, 2048.0f, 2048.0f, lightView, lightProj);
	XMMATRIX LVP = XMMatrixMultiply(lightView, lightProj);
	XMStoreFloat4x4(&passConst.LightViewProj, XMMatrixTranspose(LVP));

	//섀도 텍셀 크기, 임시 하드코딩
	passConst.ShadowTexelSize = { 1.0f / 2048.0f, 1.0f / 2048.0f };

	m_DX12PassConstantBuffer->CopyAndUploadResource(
		m_DX12PassConstantBuffer->GetResource(),
		&passConst,
		sizeof(PassConstants));
}

void DX12FrameResource::UploadObjectConstant(
	ID3D12Device* device,
	DX12CommandList* dx12CommandList,
	std::vector<Render::RenderItem>& renderItems,
	DX12ObjectConstantManager* dx12ObjectConstantManager)
{
	// Gather dirty object
	struct PendingObject
	{
		UINT index;
		ObjectConstants constantData;
	};
	std::vector<PendingObject> dirtyObjects;

	for (int i = 0; i < renderItems.size(); ++i) {
		if (!renderItems[i].IsObjectDirty())
			continue;

		// if new slot
		if (dx12ObjectConstantManager->GetObjectConstantCount() < i + 1)
		{
			dx12ObjectConstantManager->PushObjectConstant(renderItems[i].GetObjectConstant());
			renderItems[i].SetObjConstantIndex(i);
		}
		else
		{
			dx12ObjectConstantManager->GetObjectConstant(i)
				= renderItems[i].GetObjectConstant();
		}

		PendingObject tmpObject;
		tmpObject.index = i;
		tmpObject.constantData = renderItems[i].GetObjectConstant();
		dirtyObjects.push_back(tmpObject);
	}

	if (dirtyObjects.empty()) return;

	// create upload buffer
	const UINT offsetStride = sizeof(ObjectConstants);
	const UINT numDirtyObjects = SizeToU32(dirtyObjects.size());
	dx12ObjectConstantManager->CreateObjectConstantUploadBuffer(device, numDirtyObjects* offsetStride);

	// stage dirty objects on buffer
	for (auto& object : dirtyObjects)
	{
		const UINT dstOffset = object.index * offsetStride;
		dx12ObjectConstantManager->StageObjectConstants(&object.constantData, offsetStride, dstOffset);

		renderItems[object.index].SetDirtyFlag(false);
	}

	// record
	dx12ObjectConstantManager->RecordObjectConstants(dx12CommandList);
}

void DX12FrameResource::EnsureWorkerCapacity(ID3D12Device* device, uint32_t n) {
	if (m_workerAlloc.size() >= n) return;

	size_t old = m_workerAlloc.size();
	m_workerAlloc.resize(n);
	for (size_t i = old; i < n; ++i)
	{
		ThrowIfFailed(device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(m_workerAlloc[i].GetAddressOf())));
	}
}

void DX12FrameResource::ResetAllAllocators()
{
	m_commandAllocator->Reset();
	for (auto& a : m_workerAlloc) a->Reset();
}