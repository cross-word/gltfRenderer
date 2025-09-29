#pragma once

#include "stdafx.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12ROOTSIGNATURE
MAIN WORK:
1. create RootSignature
IMPORTANT MEMBER:
m_rootSignature
*/
class DX12RootSignature
{
public:
	DX12RootSignature();
	~DX12RootSignature();
	void Initialize(ID3D12Device* device);
	inline ID3D12RootSignature* GetRootSignature() const noexcept { return m_rootSignature.Get(); }

private:
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

private:
	ComPtr<ID3D12RootSignature> m_rootSignature;
};