#pragma once

#include "stdafx.h"
#include "DX12Resource.h"
#include "DX12View.h"
#include "DX12DescriptorHeap.h"
#include "D3DCamera.h"
#include "DX12ConstantManager.h"
#include "DX12RenderGeometry.h"
#include "D3DTimer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
namespace Render { struct RenderItem; }

/*
CLASS DX12FRAMERESOURCE
MAIN WORK:
1. represents the resources required by the CPU to build a list of instructions in a frame
IMPORTANT MEMBER:
1. m_commandAllocator, m_workerAlloc
2. m_DX12PassConstantBuffer
3. m_DX12PassConstantBufferView
4. m_passConstant
*/
struct DX12FrameResource
{
public:
    DX12FrameResource(ID3D12Device* device);
    DX12FrameResource(const DX12FrameResource& rhs) = delete;
    DX12FrameResource& operator=(const DX12FrameResource& rhs) = delete;
    ~DX12FrameResource();

    inline ID3D12CommandAllocator* GetCommandAllocator() const noexcept { return m_commandAllocator.Get(); }
    inline UINT64 GetFenceValue() const noexcept  { return m_fence; }
    inline void SetFenceValue(uint64_t newFenceValue) { m_fence = newFenceValue; }
    inline DX12ResourceBuffer* GetDX12PassConstantBuffer() const noexcept { return m_DX12PassConstantBuffer.get(); }
    inline DX12View* GetDX12PassConstantBufferView() const noexcept { return m_DX12PassConstantBufferView.get(); }
    void ResetAllocator() { ThrowIfFailed(m_commandAllocator->Reset()); }
    void CreateSRV(ID3D12Device* device, DX12DescriptorHeap* dx12DescriptorHeap, uint32_t frameIndex);

    void UploadPassConstant(D3DCamera* d3dCamera, std::vector<Light>& lights, D3DTimer d3dTimer);
    void UploadObjectConstant(
        ID3D12Device* device,
        DX12CommandList* dx12CommandList,
        std::vector<Render::RenderItem>& renderItems,
        DX12ObjectConstantManager* dx12ObjectConstantManager);

    //prepare for multi-thread
    void EnsureWorkerCapacity(ID3D12Device* device, uint32_t workerCount);
    ID3D12CommandAllocator* GetWorkerCommandAllocator(uint32_t i) { return m_workerAlloc[i].Get(); }
    void ResetAllAllocators();
    inline ID3D12CommandAllocator* GetCommandAllocator(uint32_t workerIndex) const { assert(workerIndex < m_workerAlloc.size()); return m_workerAlloc[workerIndex].Get(); }
    size_t GetWorkerAllocatorCount() const { return m_workerAlloc.size(); }

private:
    ComPtr<ID3D12CommandAllocator> m_commandAllocator; //for single-thread or main thread in multi-thread setting
    std::unique_ptr<DX12ResourceBuffer> m_DX12PassConstantBuffer; //b0
    std::unique_ptr<DX12View> m_DX12PassConstantBufferView;

    PassConstants m_passConstant;
    uint64_t m_fence = 0;

    std::vector<ComPtr<ID3D12CommandAllocator>> m_workerAlloc; //for multi-thread workers
};