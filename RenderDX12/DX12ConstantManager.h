#pragma once

#include "stdafx.h"

#include "D3DUtil.h"
#include "DX12Resource.h"
#include "DX12View.h"

/*
CLASS DX12CONSTANTMANAGER and their children.
MAIN WORK:
1. hold and manage constant (material, object)
2. upload constants
*/
class DX12ConstantManager
{
public:
	DX12ConstantManager();
	~DX12ConstantManager();

	virtual void InitialzieUploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT numConstants);
	virtual void InitializeSRV(ID3D12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, UINT numConstants, UINT byteStirde);
	void UploadConstant(ID3D12Device* device, DX12CommandList* dx12CommandList, UINT byteSize, const void* sourceAddress);

	inline DX12ResourceTexture* GetMaterialResource() const noexcept { return m_DX12ConstantUploader.get(); }
	inline DX12View* GetDX12MaterialView() const noexcept { return m_DX12UploaderView.get(); }
protected:
    std::unique_ptr<DX12ResourceTexture> m_DX12ConstantUploader;
    std::unique_ptr<DX12View> m_DX12UploaderView;
};

class DX12MaterialConstantManager : public DX12ConstantManager
{ 
public:
	DX12MaterialConstantManager();
	~DX12MaterialConstantManager();
	void InitialzieUploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, UINT byteSize) override;
	void InitializeSRV(ID3D12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, UINT numConstants, UINT byteStirde) override;

	void PushMaterial(std::unique_ptr<Material>&& material);
	inline size_t GetMaterialCount() const noexcept { return m_materials.size(); }
	inline Material* GetMaterial(UINT index) noexcept { return m_materials[index].get(); }
	inline bool IsMaterailEmpty() const noexcept { return m_materials.empty(); }
	inline const MaterialConstants* GetMaterialConstantData() const noexcept { return m_materialConstant.data(); }

private:
	std::vector<std::unique_ptr<Material>> m_materials;
	std::vector<MaterialConstants> m_materialConstant;
};

class DX12ObjectConstantManager : public DX12ConstantManager
{
public:
	struct ConstantCopyRegion
	{
		uint64_t srcOffset;
		uint64_t dstOffset;
		uint64_t byteSize;
	};

	DX12ObjectConstantManager();
	~DX12ObjectConstantManager();
	void InitialzieUploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, UINT numConstants) override;
	void InitializeSRV(ID3D12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, UINT numConstants, UINT byteStirde) override;

	void PushObjectConstant(ObjectConstants objectConstant);
	void CreateObjectConstantUploadBuffer(ID3D12Device* device, UINT totalBytes);
	void StageObjectConstants(const void* src, UINT byteSize, UINT dstOffset);
	void RecordObjectConstants(DX12CommandList* dx12CommandList);

	inline size_t GetObjectConstantCount() const noexcept { return m_objectConstants.size(); }
	inline ObjectConstants& GetObjectConstant(UINT index) noexcept { return m_objectConstants[index]; }
	inline bool IsobjectConstantEmpty() const noexcept { return m_objectConstants.empty(); }
	inline const ObjectConstants* GetObjectConstantData() const noexcept { return m_objectConstants.data(); }

private:
	std::vector<ObjectConstants> m_objectConstants;

	std::vector<ConstantCopyRegion> m_regions;
	std::byte* m_mappedBase = nullptr; //need bytewise offset
	UINT m_cursor = 0;
	UINT m_capacity = 0;
};