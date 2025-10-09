#pragma once

#include "stdafx.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

/*
CLASS DX12PSO
MAIN WORK:
1. manage PSOs
IMPORTANT MEMBER:
1. m_pipelineStates
*/
class DX12PSO
{
public:
	DX12PSO();
	~DX12PSO();
	enum class MainPSO : uint32_t
	{
		Opaque_BackCull = 0,
		Opaque_NoCull = 1,
		Mask_BackCull = 2,
		Mask_NoCull = 3,
		Blend_BackCull = 4,
		Blend_NoCull = 5,
		_Count
	};
	ID3D12PipelineState* GetPipelineState(MainPSO k);

	void CreateMainPassPSO(
		ID3D12Device* device,
		const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout,
		ID3D12RootSignature* rootSignature,
		DXGI_FORMAT depthStencilFormat,
		DXGI_FORMAT renderTargetFormat,
		ID3DBlob* vertexShader,
		ID3DBlob* pixelShader,
		UINT numRenderTarget,
		UINT sampleCount);
	void CreateMainPassPSOs(
		ID3D12Device* device,
		const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout,
		ID3D12RootSignature* rootSignature,
		DXGI_FORMAT depthStencilFormat,
		DXGI_FORMAT renderTargetFormat,
		ID3DBlob* vertexShader,
		ID3DBlob* pixelShader,
		ID3DBlob* pixelShaderMask,
		UINT numRenderTarget,
		UINT sampleCount);
	void CreateShadowPassPSO(
		ID3D12Device* device,
		const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout,
		ID3D12RootSignature* rootSignature,
		DXGI_FORMAT depthStencilFormat,
		DXGI_FORMAT renderTargetFormat,
		ID3DBlob* vertexShader,
		ID3DBlob* pixelShader,
		UINT numRenderTarget,
		UINT sampleCount);
	ID3D12PipelineState* GetPipelineState();
	ID3D12PipelineState* GetPipelineState(int index);
private:
	std::vector<ComPtr<ID3D12PipelineState>> m_pipelineStates;
};