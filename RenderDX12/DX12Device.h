#pragma once

#include "stdafx.h"
#include "DX12CommandList.h"
#include "DX12RootSignature.h"
#include "DX12PSO.h"
#include "DX12Resource.h"
#include "DX12View.h"
#include "DX12DescriptorHeap.h"
#include "DX12SwapChain.h"
#include "DX12FrameResource.h"
#include "DX12RenderGeometry.h"
#include "DX12DDSManager.h"
#include "DX12ConstantManager.h"
#include "D3DCamera.h"
#include "../FileLoader/GLTFLoader.h"
#include "DX12TextureManager.h"
#include "DX12ShadowManager.h"
#include "D3DTimer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
namespace Render { struct RenderItem; }
/*
CLASS DX12Device
MAIN WORK:
1. initialize D3D12device
2. initialize and manage DX12Resource/DescriptorHeaps/PSO/Views/SwapChain/CommandList
3. communicate with RenderDX12 and DX12FrameBuffer directly to make them access Resources safely
IMPORTANT MEMBER:
1. m_DX12 members
*/
class DX12Device
{
public:
	DX12Device();
	~DX12Device();
	void Initialize(HWND hWnd, const std::wstring& sceneFilePath);

	inline ID3D12Device* GetDevice() const noexcept { return m_device.Get(); }
	inline DX12DescriptorHeap* GetDX12RTVHeap() const noexcept { return m_DX12RTVHeap.get(); }
	inline DX12DescriptorHeap* GetDX12CBVSRVHeap() const noexcept { return m_DX12SRVHeap.get(); }
	inline DX12DescriptorHeap* GetDX12ImGuiHeap() const noexcept { return m_DX12ImGuiHeap.get(); }
	inline DX12DescriptorHeap* GetDX12DSVHeap() const noexcept { return m_DX12DSVHeap.get(); }
	inline DX12CommandList* GetDX12CommandList() const noexcept { return m_DX12CommandList.get(); }
	inline DX12RootSignature* GetDX12RootSignature() const noexcept { return m_DX12RootSignature.get(); }
	inline DX12PSO* GetDX12PSO() const noexcept { return m_DX12PSO.get(); }
	inline Render::RenderItem GetDX12RenderItem(UINT index) const noexcept { return m_renderItems[index]; }
	inline size_t GetRenderItemSize() const noexcept { return m_renderItems.size(); }
	inline DX12SwapChain* GetDX12SwapChain() const noexcept { return m_DX12SwapChain.get(); }
	inline HANDLE GetFenceEvent() const noexcept { return m_fenceEvent; }
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetOffsetCPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE start, UINT index, UINT handleIncrementSize)
	{
		start.ptr += SIZE_T(index) * handleIncrementSize;
		return start;
	}
	inline D3DCamera* GetD3DCamera() const noexcept { return m_camera.get(); }
	inline UINT GetCurrentBackBufferIndex() const noexcept { return m_currBackBufferIndex; }
	inline DX12FrameResource* GetFrameResource(UINT currBackBufferIndex) const noexcept { return m_DX12FrameResource[currBackBufferIndex].get(); }
	inline size_t GetTextureCount() const noexcept { return m_sceneData.textures.size(); }
	inline DX12ShadowManager* GetDX12ShadowManager() const noexcept { return m_DX12ShadowManager.get(); }
	inline void SetCurrentBackBufferIndex(UINT newIndex) { m_currBackBufferIndex = newIndex; }

	void PrepareInitialResource();
	void UpdateFrameResource(D3DTimer d3dTimer);
	UINT GetTextureIndexAsTextureName(const std::string textureName);
	UINT GetMaterialIndexAsMaterialName(const std::string materialName);

	//for multi-thread
	inline DX12CommandList* GetWorkerDX12CommandList(uint32_t workerIndex) const { assert(workerIndex < m_workerDX12CommandList.size()); return m_workerDX12CommandList[workerIndex].get(); }
	inline size_t GetWorkerDX12CommandListSize() const { return m_workerDX12CommandList.size(); }
	inline DX12CommandList* GetPostDrawDX12CommandList() const noexcept { return m_postDrawDX12CommandList.get(); }
	inline DX12CommandList* GetWorkerShadowDX12CommandList(uint32_t workerIndex) const { assert(workerIndex < m_workerShadowDX12CommandList.size()); return m_workerShadowDX12CommandList[workerIndex].get(); }
	inline size_t GetWorkerShadowDX12CommandListSize() const { return m_workerShadowDX12CommandList.size(); }
private:
	void InitDX12CommandList(ID3D12CommandAllocator* commandAllocator);
	void InitDX12SwapChain(HWND hWnd);
	void InitDX12RTVDescHeap();
	void InitDX12DSVDescHeap();
	void InitDX12SRVHeap();
	void InitDX12ImGuiHeap();
	void InitDX12RootSignature();
	void CreateDX12PSO();
	void InitShader();

	void InitDX12FrameResource();
	void CreateDX12FrameResourceSRV();
	void InitDX12ShadowManager();
private:
	ComPtr<IDXGIFactory4> m_factory;
	ComPtr<ID3D12Device> m_device;
	std::unique_ptr<DX12CommandList> m_DX12CommandList; //for single-thread or main thread in multi-thread setting
	std::unique_ptr<DX12RootSignature> m_DX12RootSignature;
	std::unique_ptr<DX12PSO> m_DX12PSO;

	//initial resources
	uint32_t m_currBackBufferIndex = 0;
	std::vector<std::unique_ptr<DX12FrameResource>> m_DX12FrameResource;
	std::vector<std::unique_ptr<DX12RenderGeometry>> m_DX12RenderGeometry;
	std::vector<std::unique_ptr<DX12TextureManager>> m_DX12TextureManager;
	std::unique_ptr<DX12MaterialConstantManager> m_DX12MaterialConstantManager;

	std::unique_ptr<DX12SwapChain> m_DX12SwapChain;

	std::unique_ptr<DX12DescriptorHeap> m_DX12RTVHeap;
	std::unique_ptr<DX12DescriptorHeap> m_DX12SRVHeap; //CBVSRV
	std::unique_ptr<DX12DescriptorHeap> m_DX12ImGuiHeap; //Imgui-SRV
	std::unique_ptr<DX12DescriptorHeap> m_DX12DSVHeap;
	std::unique_ptr<DX12ObjectConstantManager> m_DX12ObjectConstantManager;
	std::unique_ptr<DX12ShadowManager> m_DX12ShadowManager;

	ComPtr<ID3DBlob> m_vertexShader = nullptr;
	ComPtr<ID3DBlob> m_pixelShader = nullptr;
	ComPtr<ID3DBlob> m_shadowVertexShader = nullptr;
	ComPtr<ID3DBlob> m_shadowPixelShader = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

	std::unique_ptr <D3DCamera> m_camera = std::make_unique<D3DCamera>();
	HANDLE m_fenceEvent = nullptr;

	std::vector<Render::RenderItem> m_renderItems;

	SceneData m_sceneData;
	std::vector<std::unique_ptr<DX12RenderGeometry>> m_sceneGeometry;

	//for multi-thread
	std::vector<std::unique_ptr<DX12CommandList>> m_workerDX12CommandList;
	std::unique_ptr<DX12CommandList> m_postDrawDX12CommandList; //command list to finish frame
	std::vector<std::unique_ptr<DX12CommandList>> m_workerShadowDX12CommandList;

};