#pragma once

#include "stdafx.h"
#include "D3DUtil.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12DESCRIPTORHEAP
MAIN WORK:
1. create DescriptorHeap
IMPORTANT MEMBER:
1. m_descHeap
*/
class DX12DescriptorHeap
{
public:
	DX12DescriptorHeap();
	~DX12DescriptorHeap();

	void Initialize(
		ID3D12Device* device,
		uint32_t numDesc,
		D3D12_DESCRIPTOR_HEAP_TYPE descHeapType,
		D3D12_DESCRIPTOR_HEAP_FLAGS descFlag,
		uint32_t nodeMask = 0);

	inline ID3D12DescriptorHeap* GetDescHeap() const noexcept { return m_descHeap.Get(); }
	inline uint32_t GetDescIncSize() const noexcept { return m_descriptorIncrementSize; }
	HeapSlice Offset(uint32_t index) const;

	inline uint32_t CalcHeapSliceShareBlock(
		uint32_t frameIndex, //Nth frame
		uint32_t threadIndex, //Nth thread
		uint32_t numDescPerFrame, //number of public descriptors per Frame (which threads shares)
		uint32_t numDescPerThread, //number of private descriptors per Thread
		uint32_t maxWorkers) 
	{
		const uint32_t frameStride = numDescPerFrame + numDescPerThread * maxWorkers;
		const uint32_t base = frameIndex * frameStride;
		const uint32_t start = base + threadIndex * numDescPerThread;

		return start;
	};

	inline UINT CalcHeapSlicePrivateBlock(
		uint32_t frameIndex, //Nth frame
		uint32_t threadIndex, //Nth thread
		uint32_t numDescPerFrame, //number of public descriptors per Frame (which threads shares)
		uint32_t numDescPerThread, //number of private descriptors per Thread
		uint32_t maxWorkers)
	{
		const uint32_t frameStride = numDescPerFrame + numDescPerThread * maxWorkers;
		const uint32_t base = frameIndex * frameStride;
		const uint32_t start = base + numDescPerFrame + threadIndex * numDescPerThread;

		return start;
	};

private:
	ComPtr<ID3D12DescriptorHeap> m_descHeap;
	uint32_t m_descriptorIncrementSize = 0;
};