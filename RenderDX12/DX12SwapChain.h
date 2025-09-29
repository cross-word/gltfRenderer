#pragma once

#include "stdafx.h"
#include "DX12CommandList.h"
#include "../MiniEngineCore/EngineConfig.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12SWAPCHAIN
MAIN WORK:
1. create SwapChain
IMPORTANT MEMBER:
1. m_swapChain
*/
class DX12SwapChain
{
public:
	DX12SwapChain();
	~DX12SwapChain();

	void InitializeMultiSample(ID3D12Device* device);
	void InitializeSwapChain(DX12CommandList* dx12CommandList, IDXGIFactory4* factory, HWND hWnd);
	inline IDXGISwapChain3* GetSwapChain() const noexcept { return m_swapChain.Get(); }
	inline UINT GetSwapChainBufferCount() const noexcept { return m_swapChainBufferCount; }
	inline DXGI_FORMAT GetRenderTargetFormat() const noexcept { return m_renderTargetFormat; }
	inline DXGI_FORMAT GetBackBufferFormat() const noexcept { return m_backBufferFormat; }
	inline DXGI_FORMAT GetDepthStencilFormat() const noexcept { return m_depthStencilFormat; }
	inline int GetClientWidth() const noexcept { return m_clientWidth; }
	inline int GetClientHeight() const noexcept { return m_clientHeight; }
	inline UINT GetMsaaQuality() const noexcept { return m_8xMsaaQuality; }
	inline UINT GetMsaaSampleCount() const noexcept { return m_8xMsaaSampleCount; }
private:
	UINT m_swapChainBufferCount = EngineConfig::SwapChainBufferCount;
	ComPtr<IDXGISwapChain3> m_swapChain;

	DXGI_FORMAT m_renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	UINT m_clientWidth = EngineConfig::DefaultWidth;
	UINT m_clientHeight = EngineConfig::DefaultHeight;

	UINT m_8xMsaaQuality = 0;
	UINT m_8xMsaaSampleCount = EngineConfig::MsaaSampleCount;
};