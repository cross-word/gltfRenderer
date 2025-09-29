#pragma once
#include "stdafx.h"

using Microsoft::WRL::ComPtr;
/*
CLASS D3DTIMER
MAIN WORK:
1. measure CPU Time in rendering pipeline (working time except waiting for GPU work)
2. measure GPU Time in rendering pipeline (working time except waiting for CPU work)
IMPORTANT MEMBER:
*/
class D3DTimer
{
public:
    void Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue);
    // GPU timing -----------------------------------------------------------
    void BeginGPU(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex);
    void EndGPU(ID3D12GraphicsCommandList* commandList, uint32_t frameIndex);
    float GetElapsedGPUMS(uint32_t frameIndex);

    // CPU timing -----------------------------------------------------------
    void BeginCPU(uint32_t frameIndex);
    void EndCPU(uint32_t frameIndex);
    float GetElapsedCPUMS(uint32_t frameIndex) const;

    // total timer
    void SetTotalTime(float deltaTime) { m_totalTime += deltaTime; }
    float GetTotalTime() const noexcept { return m_totalTime; }
private:
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_readbackBuffer;
    UINT64 m_frequency = 0;

    std::vector<LARGE_INTEGER> m_cpuBegin;
    std::vector<float>        m_cpuElapsedMS;
    LARGE_INTEGER             m_cpuFreq{};
    float m_totalTime = 0;
};