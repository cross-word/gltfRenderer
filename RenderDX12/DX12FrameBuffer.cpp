#include "stdafx.h"
#include "DX12FrameBuffer.h"

DX12FrameBuffer::DX12FrameBuffer()
{

}

DX12FrameBuffer::~DX12FrameBuffer()
{

}

void DX12FrameBuffer::Initialize(DX12Device* dx12Device)
{
	assert(dx12Device);
	for (UINT i = 0; i < dx12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); ++i)
	{
		m_DX12DepthStencils[i] = std::make_unique<DX12ResourceTexture>();
		m_DX12RenderTargets[i] = std::make_unique<DX12Resource>();
		m_DX12MsaaDepthStencils[i] = std::make_unique<DX12ResourceTexture>();
		m_DX12MsaaRenderTargets[i] = std::make_unique<DX12ResourceTexture>();
	}
}

void DX12FrameBuffer::Resize(DX12Device* dx12Device)
{
	// Release the previous resources we will be recreating.
	for (UINT i = 0; i < dx12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); ++i)
	{
		if (m_DX12RenderTargets[i]->GetResource() != nullptr) m_DX12RenderTargets[i]->Reset();
		if (m_DX12DepthStencils[i]->GetResource() != nullptr) m_DX12DepthStencils[i]->Reset();
		if (m_DX12MsaaDepthStencils[i]->GetResource() != nullptr) m_DX12MsaaDepthStencils[i]->Reset(); msaaDSVOffsetHandle[i] = {};
		if (m_DX12MsaaRenderTargets[i]->GetResource() != nullptr) m_DX12MsaaRenderTargets[i]->Reset(); msaaRTVOffsetHandle[i] = {};
	}

	// Resize the swap chain.
	ThrowIfFailed(dx12Device->GetDX12SwapChain()->GetSwapChain()->ResizeBuffers(
		dx12Device->GetDX12SwapChain()->GetSwapChainBufferCount(),
		dx12Device->GetDX12SwapChain()->GetClientWidth(),
		dx12Device->GetDX12SwapChain()->GetClientHeight(),
		dx12Device->GetDX12SwapChain()->GetBackBufferFormat(),
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	CreateRenderTargetsAndViews(dx12Device);
	CreateDepthStencilAndView(dx12Device);
	CreateMsaaRenderTargetAndView(dx12Device);
	CreateMsaaDepthStencilAndView(dx12Device);
	SetViewPortAndScissor(dx12Device);

	// Execute the resize commands.
	dx12Device->GetDX12CommandList()->SubmitAndWait();
}

void DX12FrameBuffer::CreateRenderTargetsAndViews(DX12Device* dx12Device)
{
	//create RTV
	CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHeapHandle(
		dx12Device->GetDX12RTVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < dx12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); i++)
	{
		ThrowIfFailed(dx12Device->GetDX12SwapChain()->GetSwapChain()->GetBuffer(i, IID_PPV_ARGS(m_DX12RenderTargets[i]->GetAddressOf())));
		m_DX12RenderTargets[i]->TransitionState(dx12Device->GetDX12CommandList(), D3D12_RESOURCE_STATE_PRESENT);
		m_DX12RenderTargetViews[i] = std::make_unique<DX12View>(
			dx12Device->GetDevice(),
			EViewType::ERenderTargetView,
			m_DX12RenderTargets[i].get(),
			RTVHeapHandle);
		RTVHeapHandle.Offset(1, dx12Device->GetDX12RTVHeap()->GetDescIncSize());
	}
}

void DX12FrameBuffer::CreateDepthStencilAndView(DX12Device* dx12Device)
{
	//create DSV
	CD3DX12_CPU_DESCRIPTOR_HANDLE DSVHeapHandle(
		dx12Device->GetDX12DSVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart()
	);
	for (UINT i = 0; i < dx12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); i++)
	{
		m_DX12DepthStencils[i]->CreateDepthStencil(
			dx12Device->GetDevice(),
			dx12Device->GetDX12SwapChain()->GetClientWidth(),
			dx12Device->GetDX12SwapChain()->GetClientHeight(),
			dx12Device->GetDX12SwapChain()->GetMsaaSampleCount(),
			dx12Device->GetDX12SwapChain()->GetDepthStencilFormat());
		m_DX12DepthStencilViews[i] = std::make_unique<DX12View>(
			dx12Device->GetDevice(),
			EViewType::EDepthStencilView,
			m_DX12DepthStencils[i].get(),
			DSVHeapHandle);
		m_DX12DepthStencils[i]->TransitionState(dx12Device->GetDX12CommandList(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		DSVHeapHandle.Offset(1, dx12Device->GetDX12DSVHeap()->GetDescIncSize());
	}
}

void DX12FrameBuffer::CreateMsaaRenderTargetAndView(DX12Device* dx12Device)
{
	//create MSAA RTV
	CD3DX12_CPU_DESCRIPTOR_HANDLE tmpMsaaRTVOffsetHandle = static_cast<CD3DX12_CPU_DESCRIPTOR_HANDLE>(
		dx12Device->GetOffsetCPUHandle(
			dx12Device->GetDX12RTVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
			dx12Device->GetDX12SwapChain()->GetSwapChainBufferCount(),
			dx12Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)));
	for (UINT i = 0; i < dx12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); i++)
	{
		msaaRTVOffsetHandle[i] = tmpMsaaRTVOffsetHandle;
		m_DX12MsaaRenderTargets[i]->CreateRenderTarget(
			dx12Device->GetDevice(),
			dx12Device->GetDX12SwapChain()->GetClientWidth(),
			dx12Device->GetDX12SwapChain()->GetClientHeight(),
			dx12Device->GetDX12SwapChain()->GetMsaaSampleCount(),
			dx12Device->GetDX12SwapChain()->GetRenderTargetFormat());
		m_DX12MsaaRenderTargetViews[i] = std::make_unique<DX12View>(
			dx12Device->GetDevice(),
			EViewType::ERenderTargetView,
			m_DX12MsaaRenderTargets[i].get(),
			tmpMsaaRTVOffsetHandle);
		tmpMsaaRTVOffsetHandle.Offset(1, dx12Device->GetDX12RTVHeap()->GetDescIncSize());
	}
}

void DX12FrameBuffer::CreateMsaaDepthStencilAndView(DX12Device* dx12Device)
{
	//create MSAA DSV
	CD3DX12_CPU_DESCRIPTOR_HANDLE tmpMsaaDSVOffsetHandle = static_cast<CD3DX12_CPU_DESCRIPTOR_HANDLE>(
		dx12Device->GetOffsetCPUHandle(
			dx12Device->GetDX12DSVHeap()->GetDescHeap()->GetCPUDescriptorHandleForHeapStart(),
			dx12Device->GetDX12SwapChain()->GetSwapChainBufferCount(),
			dx12Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)));

	for (UINT i = 0; i < dx12Device->GetDX12SwapChain()->GetSwapChainBufferCount(); i++)
	{
		msaaDSVOffsetHandle[i] = tmpMsaaDSVOffsetHandle;
		m_DX12MsaaDepthStencils[i]->CreateDepthStencil(
			dx12Device->GetDevice(),
			dx12Device->GetDX12SwapChain()->GetClientWidth(),
			dx12Device->GetDX12SwapChain()->GetClientHeight(),
			dx12Device->GetDX12SwapChain()->GetMsaaSampleCount(),
			dx12Device->GetDX12SwapChain()->GetDepthStencilFormat());
		m_DX12MsaaDepthStencilViews[i] = std::make_unique<DX12View>(
			dx12Device->GetDevice(),
			EViewType::EDepthStencilView,
			m_DX12MsaaDepthStencils[i].get(),
			tmpMsaaDSVOffsetHandle);
		m_DX12MsaaDepthStencils[i]->TransitionState(dx12Device->GetDX12CommandList(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		tmpMsaaDSVOffsetHandle.Offset(1, dx12Device->GetDX12DSVHeap()->GetDescIncSize());
	}
}

void DX12FrameBuffer::SetViewPortAndScissor(DX12Device* dx12Device)
{
	// Update the viewport transform to cover the client area.
	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;
	m_viewport.Width = static_cast<float>(dx12Device->GetDX12SwapChain()->GetClientWidth());
	m_viewport.Height = static_cast<float>(dx12Device->GetDX12SwapChain()->GetClientHeight());
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_scissor = { 0, 0, dx12Device->GetDX12SwapChain()->GetClientWidth(), dx12Device->GetDX12SwapChain()->GetClientHeight() };
}

void DX12FrameBuffer::CheckFence(DX12Device* dx12Device, UINT currBackBufferIndex)
{
	if (dx12Device->GetDX12CommandList()->GetFence()->GetCompletedValue() < dx12Device->GetFrameResource(currBackBufferIndex)->GetFenceValue())
	{
		HANDLE h = dx12Device->GetFenceEvent();
		dx12Device->GetDX12CommandList()->GetFence()->SetEventOnCompletion(dx12Device->GetFrameResource(currBackBufferIndex)->GetFenceValue(), h);
		WaitForSingleObject(h, INFINITE);
	}
}

void DX12FrameBuffer::BeginMainPass(DX12CommandList* dx12CommandList, UINT currBackBufferIndex)
{
	//begin main render
	dx12CommandList->GetCommandList()->ClearRenderTargetView(msaaRTVOffsetHandle[currBackBufferIndex], EngineConfig::DefaultClearColor, 0, nullptr);
	dx12CommandList->GetCommandList()->ClearDepthStencilView(msaaDSVOffsetHandle[currBackBufferIndex], D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
}

void DX12FrameBuffer::SetMainPassRenderViewPort(DX12CommandList* dx12CommandList, UINT currBackBufferIndex)
{
	dx12CommandList->GetCommandList()->RSSetViewports(1, &m_viewport);
	dx12CommandList->GetCommandList()->RSSetScissorRects(1, &m_scissor);
	dx12CommandList->GetCommandList()->OMSetRenderTargets(1, &msaaRTVOffsetHandle[currBackBufferIndex], FALSE, &msaaDSVOffsetHandle[currBackBufferIndex]);
}


void DX12FrameBuffer::EndMainPass(
	DX12CommandList* dx12CommandList,
	UINT currBackBufferIndex,
	DXGI_FORMAT renderTargetFormat,
	DX12ShadowManager* dx12ShadowManager)
{
	//MSAA RESOLVE
	//MsaaRTv : RT->ResolveSource
	//BackBuffer : RT->ResolveDest
	m_DX12MsaaRenderTargets[currBackBufferIndex]->TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
	m_DX12RenderTargets[currBackBufferIndex]->TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_RESOLVE_DEST);
	dx12CommandList->RecordResourceStateTransition();

	dx12CommandList->GetCommandList()->ResolveSubresource(
		m_DX12RenderTargets[currBackBufferIndex]->GetResource(),
		0,
		m_DX12MsaaRenderTargets[currBackBufferIndex]->GetResource(),
		0,
		renderTargetFormat);

	//MsaaRTv : ResolveSource->RT
	//BackBuffer : ResolveDest->RT // FOR TIMER
	m_DX12MsaaRenderTargets[currBackBufferIndex]->TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_DX12RenderTargets[currBackBufferIndex]->TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	dx12CommandList->RecordResourceStateTransition();
}

void DX12FrameBuffer::SetBackBufferPresent(DX12CommandList* dx12CommandList, UINT currBackBufferIndex)
{
	m_DX12RenderTargets[currBackBufferIndex]->TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_PRESENT);
	dx12CommandList->RecordResourceStateTransition();
}

void DX12FrameBuffer::Present(DX12Device* dx12Device)
{
	ThrowIfFailed(dx12Device->GetDX12SwapChain()->GetSwapChain()->Present(1, 0));
}

void DX12FrameBuffer::BeginShadowRender(DX12CommandList* dx12CommandList, DX12ShadowManager* dx12ShadowManager, CD3DX12_CPU_DESCRIPTOR_HANDLE shadowDepthStencilCPUHandle)
{
	//begin shadow
	dx12ShadowManager->GetShadowResource()->TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	dx12CommandList->RecordResourceStateTransition();
	dx12CommandList->GetCommandList()->ClearDepthStencilView(shadowDepthStencilCPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
}

void DX12FrameBuffer::EndShadowRender(DX12CommandList* dx12CommandList, DX12ShadowManager* dx12ShadowManager)
{
	//shadow state reset
	dx12ShadowManager->GetShadowResource()->TransitionState(dx12CommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	dx12CommandList->RecordResourceStateTransition();
}

void DX12FrameBuffer::SetShadowRenderViewPort(DX12CommandList* dx12CommandList, DX12ShadowManager* dx12ShadowManager, D3D12_CPU_DESCRIPTOR_HANDLE shadowDepthStencilCPUHandle)
{
	UINT shadowWidth = dx12ShadowManager->GetShadowWidth();
	UINT shadowHeight = dx12ShadowManager->GetShadowHeight();
	D3D12_VIEWPORT shadowViewPort{ 0.0f, 0.0f, (float)shadowWidth, (float)shadowHeight, 0.0f, 1.0f };
	D3D12_RECT shadowScissor{ 0, 0, (LONG)shadowWidth, (LONG)shadowHeight };
	dx12CommandList->GetCommandList()->RSSetViewports(1, &shadowViewPort);
	dx12CommandList->GetCommandList()->RSSetScissorRects(1, &shadowScissor);
	dx12CommandList->GetCommandList()->OMSetRenderTargets(0, nullptr, FALSE, &shadowDepthStencilCPUHandle);
}