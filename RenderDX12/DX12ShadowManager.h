#pragma once

#include "stdafx.h"
#include "D3DUtil.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#include "DX12Resource.h"
#include "DX12View.h"

/*
CLASS DX12SHADOWMANAGER
MAIN WORK:
1. create shadow resource and view
*/
class DX12ShadowManager
{
public:
	DX12ShadowManager();
	~DX12ShadowManager();

	void Initialzie(ID3D12Device* device, DX12CommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsvCpuHandle, D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle);

	inline DXGI_FORMAT GetShadowResourceFormat() const noexcept { return m_shadowResourceFormat; }
	inline DXGI_FORMAT GetShadowDepthStencilFormat() const noexcept { return m_shadowDepthStencilFormat; }
	inline DXGI_FORMAT GetShadowShaderResourceFormat() const noexcept { return m_shadowShaderResourceFormat; }
	inline DX12ResourceTexture* GetShadowResource() const noexcept { return m_DX12ShadowResource.get(); }
	inline UINT GetShadowWidth() const noexcept { return m_shadowWidth; }
	inline UINT GetShadowHeight() const noexcept { return m_shadowHeight; }
private:
	std::unique_ptr<DX12ResourceTexture> m_DX12ShadowResource;
	std::unique_ptr<DX12View> m_DX12ShadowDescriptorView;
	std::unique_ptr<DX12View> m_DX12ShadowResourceView;


	UINT m_shadowWidth = 2048;
	UINT m_shadowHeight = 2048;

	DXGI_FORMAT m_shadowResourceFormat = DXGI_FORMAT_R32_TYPELESS;
	DXGI_FORMAT m_shadowDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
	DXGI_FORMAT m_shadowShaderResourceFormat = DXGI_FORMAT_R32_FLOAT;
};