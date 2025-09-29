#pragma once
#include "stdafx.h"
#include "DX12Device.h"
#include "DX12Resource.h"
#include "DX12View.h"
#include "../MiniEngineCore/EngineConfig.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12FRAMEBUFFER
MAIN WORK:
1. command For Frame Drawing
2. create And Manage Components for Drawing frame
3. communicate with RenderDX12 directly for Drawing (prepare render target/ depth stencil/ veiwports, and resources state)
IMPORTANT MEMBER:
1. m_renderTargets
2. m_depthStencil
*/
class DX12FrameBuffer
{
public:
    DX12FrameBuffer();
    ~DX12FrameBuffer();


    void Initialize(DX12Device* dx12Device);
    void Resize(DX12Device* dx12Device);
    void CheckFence(DX12Device* dx12Device, UINT currBackBufferIndex);
    void BeginMainPass(
        DX12CommandList* dx12CommandList,
        UINT currBackBufferIndex);
    void SetMainPassRenderViewPort(DX12CommandList* dx12CommandList, UINT currBackBufferIndex);
    void EndMainPass(
        DX12CommandList* dx12CommandList,
        UINT currBackBufferIndex,
        DXGI_FORMAT renderTargetFormat,
        DX12ShadowManager* dx12ShadowManager);
    void SetBackBufferPresent(DX12CommandList* dx12CommandList, UINT currBackBufferIndex);
    void Present(DX12Device* dx12Device);

    void BeginShadowRender(DX12CommandList* dx12CommandList, DX12ShadowManager* dx12ShadowManager, CD3DX12_CPU_DESCRIPTOR_HANDLE shadowDepthStencilCPUHandle);
    void EndShadowRender(DX12CommandList* dx12CommandList, DX12ShadowManager* dx12ShadowManager);
    void SetShadowRenderViewPort(DX12CommandList* dx12CommandList, DX12ShadowManager* dx12ShadowManager, D3D12_CPU_DESCRIPTOR_HANDLE shadowDepthStencilCPUHandle);

private:
    void CreateRenderTargetsAndViews(DX12Device* dx12Device);
    void CreateDepthStencilAndView(DX12Device* dx12Device);
    void CreateMsaaRenderTargetAndView(DX12Device* dx12Device);
    void CreateMsaaDepthStencilAndView(DX12Device* dx12Device);
    void SetViewPortAndScissor(DX12Device* dx12Device);

    std::unique_ptr<DX12Resource> m_DX12RenderTargets[EngineConfig::SwapChainBufferCount];
    std::unique_ptr<DX12View> m_DX12RenderTargetViews[EngineConfig::SwapChainBufferCount];
    std::unique_ptr<DX12ResourceTexture> m_DX12DepthStencils[EngineConfig::SwapChainBufferCount];
    std::unique_ptr<DX12View> m_DX12DepthStencilViews[EngineConfig::SwapChainBufferCount];
    
    //There are same amount of Msaa objects as the swapchain count.
    std::unique_ptr<DX12ResourceTexture> m_DX12MsaaRenderTargets[EngineConfig::SwapChainBufferCount];
    std::unique_ptr<DX12View> m_DX12MsaaRenderTargetViews[EngineConfig::SwapChainBufferCount];
    std::unique_ptr<DX12ResourceTexture> m_DX12MsaaDepthStencils[EngineConfig::SwapChainBufferCount];
    std::unique_ptr<DX12View> m_DX12MsaaDepthStencilViews[EngineConfig::SwapChainBufferCount];

    CD3DX12_CPU_DESCRIPTOR_HANDLE msaaDSVOffsetHandle[EngineConfig::SwapChainBufferCount];
    CD3DX12_CPU_DESCRIPTOR_HANDLE msaaRTVOffsetHandle[EngineConfig::SwapChainBufferCount];

    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT     m_scissor{};
};