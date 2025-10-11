#include "stdafx.h"
#include "DX12PSO.h"

DX12PSO::DX12PSO()
{

}

DX12PSO::~DX12PSO()
{

}

void DX12PSO::CreateMainPassPSO(
	ID3D12Device* device,
	const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout,
	ID3D12RootSignature* rootSignature,
	DXGI_FORMAT depthStencilFormat,
	DXGI_FORMAT renderTargetFormat,
	ID3DBlob* vertexShader,
	ID3DBlob* pixelShader,
	UINT numRenderTarget,
	UINT sampleCount)
{
	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	const UINT inputLayoutSize = SizeToU32(inputLayout.size());
	psoDesc.InputLayout = { inputLayout.data(), inputLayoutSize };
	psoDesc.pRootSignature = rootSignature;
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DSVFormat = depthStencilFormat;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = numRenderTarget;
	psoDesc.RTVFormats[0] = renderTargetFormat;
	psoDesc.SampleDesc.Count = sampleCount;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.RasterizerState.MultisampleEnable = (sampleCount > 1);

	// allocate PSO to vector
	ComPtr<ID3D12PipelineState> tmpPSO;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&tmpPSO)));
	m_pipelineStates.push_back(tmpPSO);

	return;
}

void DX12PSO::CreateMainPassPSOs(
	ID3D12Device* device,
	const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout,
	ID3D12RootSignature* rootSignature,
	DXGI_FORMAT depthStencilFormat,
	DXGI_FORMAT renderTargetFormat,
	ID3DBlob* vertexShader,
	ID3DBlob* pixelShader,
	ID3DBlob* pixelShaderMask,
	UINT numRenderTarget,
	UINT sampleCount)
{
	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	const UINT inputLayoutSize = SizeToU32(inputLayout.size());
	psoDesc.InputLayout = { inputLayout.data(), inputLayoutSize };
	psoDesc.pRootSignature = rootSignature;
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DSVFormat = depthStencilFormat;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = numRenderTarget;
	psoDesc.RTVFormats[0] = renderTargetFormat;
	psoDesc.SampleDesc.Count = sampleCount;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.RasterizerState.MultisampleEnable = (sampleCount > 1);

	auto makeRaster = [&](bool noCull) {
		auto r = psoDesc.RasterizerState;
		r.CullMode = noCull ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
		return r;
		};

	auto makeBlendAlpha = []() {
		D3D12_BLEND_DESC b = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		b.RenderTarget[0].BlendEnable = TRUE;
		b.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		b.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		b.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		b.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		b.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		b.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		return b;
		};

	auto create = [&](MainPSO key, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) {
		ComPtr<ID3D12PipelineState> p;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&p)));
		m_pipelineStates.push_back(p);
		};

	// Opaque Back/NoCull
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
	psoDesc.RasterizerState = makeRaster(false);
	create(MainPSO::Opaque_BackCull, psoDesc);
	psoDesc.RasterizerState = makeRaster(true);
	create(MainPSO::Opaque_NoCull, psoDesc);

	// Mask Back/NoCull
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderMask);
	psoDesc.RasterizerState = makeRaster(false);
	create(MainPSO::Mask_BackCull, psoDesc);
	psoDesc.RasterizerState = makeRaster(true);
	create(MainPSO::Mask_NoCull, psoDesc);

	// Blend Back/NoCull
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
	psoDesc.BlendState = makeBlendAlpha();
	auto ds = psoDesc.DepthStencilState; ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState = ds;
	psoDesc.RasterizerState = makeRaster(false);
	create(MainPSO::Blend_BackCull, psoDesc);
	psoDesc.RasterizerState = makeRaster(true);
	create(MainPSO::Blend_NoCull, psoDesc);
}

void DX12PSO::CreateShadowPassPSO(
	ID3D12Device* device,
	const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout,
	ID3D12RootSignature* rootSignature,
	DXGI_FORMAT depthStencilFormat,
	DXGI_FORMAT renderTargetFormat,
	ID3DBlob* vertexShader,
	ID3DBlob* pixelShader,
	UINT numRenderTarget,
	UINT sampleCount)
{
	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	const UINT inputLayoutSize = SizeToU32(inputLayout.size());
	psoDesc.InputLayout = { inputLayout.data(), inputLayoutSize };
	psoDesc.pRootSignature = rootSignature;
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DSVFormat = depthStencilFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = numRenderTarget;
	psoDesc.RTVFormats[0] = renderTargetFormat;
	psoDesc.SampleDesc.Count = sampleCount;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.RasterizerState.MultisampleEnable = (sampleCount > 1);

	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_shadowPipelineStates)));

	return;
}

ID3D12PipelineState* DX12PSO::GetPipelineState()
{
	if (m_pipelineStates.empty()) return nullptr;

	return m_pipelineStates.front().Get();
}

ID3D12PipelineState* DX12PSO::GetPipelineState(int index)
{
	assert(index < m_pipelineStates.size());

	return m_pipelineStates[index].Get();
}

ID3D12PipelineState* DX12PSO::GetPipelineState(MainPSO k)
{
	assert(uint32_t(k) < m_pipelineStates.size());

	return m_pipelineStates[uint32_t(k)].Get();
}
