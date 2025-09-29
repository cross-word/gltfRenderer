#include "stdafx.h"
#include "DX12DescriptorHeap.h"

DX12DescriptorHeap::DX12DescriptorHeap()
{

}

DX12DescriptorHeap::~DX12DescriptorHeap()
{

}

void DX12DescriptorHeap::Initialize(
	ID3D12Device* device,
	uint32_t numDesc,
	D3D12_DESCRIPTOR_HEAP_TYPE descHeapType,
	D3D12_DESCRIPTOR_HEAP_FLAGS descFlag,
	uint32_t nodeMask)
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc{};
	cbvHeapDesc.NumDescriptors = numDesc;
	cbvHeapDesc.Type = descHeapType;
	cbvHeapDesc.Flags = descFlag;
	cbvHeapDesc.NodeMask = nodeMask;
	ThrowIfFailed(device->CreateDescriptorHeap(
		&cbvHeapDesc, IID_PPV_ARGS(m_descHeap.GetAddressOf())));
	m_descriptorIncrementSize = device->GetDescriptorHandleIncrementSize(descHeapType);
}

HeapSlice DX12DescriptorHeap::Offset(uint32_t index) const
{
	HeapSlice slice{};
	slice.cpuDescHandle = m_descHeap->GetCPUDescriptorHandleForHeapStart();
	slice.cpuDescHandle.ptr += SIZE_T(index) * SIZE_T(m_descriptorIncrementSize);

	// GPU Handle is enable iff heap is Shader visible
	if (m_descHeap->GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) 
	{
		slice.gpuDescHandle = m_descHeap->GetGPUDescriptorHandleForHeapStart();
		slice.gpuDescHandle.ptr += UINT64(index) * UINT64(m_descriptorIncrementSize);
	}
	else 
	{
		slice.gpuDescHandle.ptr = 0;
	}

	return slice;
}