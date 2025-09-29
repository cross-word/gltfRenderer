#include "stdafx.h"
#include "D3DTimer.h"

void D3DTimer::Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue)
{
    m_cpuBegin.resize(EngineConfig::SwapChainBufferCount);
    m_cpuElapsedMS.resize(EngineConfig::SwapChainBufferCount, 0.0f);
    QueryPerformanceFrequency(&m_cpuFreq);

    D3D12_QUERY_HEAP_DESC heapDesc = {};
    heapDesc.Count = EngineConfig::SwapChainBufferCount * 2;
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_queryHeap));

    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * heapDesc.Count);
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&m_readbackBuffer));

    commandQueue->GetTimestampFrequency(&m_frequency);
}

void D3DTimer::BeginGPU(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex)
{
    commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, frameIndex * 2);
}

void D3DTimer::EndGPU(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex)
{
    commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, frameIndex * 2 + 1);
    commandList->ResolveQueryData(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
        frameIndex * 2, 2, m_readbackBuffer.Get(),
        frameIndex * sizeof(uint64_t) * 2);
}

float D3DTimer::GetElapsedGPUMS(uint32_t frameIndex)
{
    uint64_t* data = nullptr;
    D3D12_RANGE range{ frameIndex * 2 * sizeof(uint64_t), (frameIndex * 2 + 2) * sizeof(uint64_t) };
    if (SUCCEEDED(m_readbackBuffer->Map(0, &range, reinterpret_cast<void**>(&data))))
    {
        uint64_t start = data[frameIndex * 2];
        uint64_t end = data[frameIndex * 2 + 1];
        D3D12_RANGE empty{ 0,0 };
        m_readbackBuffer->Unmap(0, &empty);
        if (end > start && m_frequency)
        {
            return float(end - start) / static_cast<float>(m_frequency) * 1000.0f;
        }
    }
    return 0.0f;
}

void D3DTimer::BeginCPU(uint32_t frameIndex)
{
    QueryPerformanceCounter(&m_cpuBegin[frameIndex]);
}

void D3DTimer::EndCPU(uint32_t frameIndex)
{
    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);
    m_cpuElapsedMS[frameIndex] =
        static_cast<float>(end.QuadPart - m_cpuBegin[frameIndex].QuadPart) * 1000.0f /
        static_cast<float>(m_cpuFreq.QuadPart);
}

float D3DTimer::GetElapsedCPUMS(uint32_t frameIndex) const
{
    if (frameIndex < m_cpuElapsedMS.size())
        return m_cpuElapsedMS[frameIndex];
    return 0.0f;
}