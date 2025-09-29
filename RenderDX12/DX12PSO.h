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