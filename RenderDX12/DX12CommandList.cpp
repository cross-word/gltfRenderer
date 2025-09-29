#include "stdafx.h"
#include "DX12CommandList.h"


DX12CommandList::DX12CommandList()
{

}

DX12CommandList::~DX12CommandList()
{
	//if (m_fenceEvent) CloseHandle(m_fenceEvent);
}

void DX12CommandList::Initialize(ID3D12Device* device, ID3D12CommandAllocator* commandAllocator, HANDLE fenceEvent)
{
	//create command queue/allocator
	D3D12_COMMAND_QUEUE_DESC m_queueDesc = {};
	m_queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	m_queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	ThrowIfFailed(device->CreateCommandQueue(
		&m_queueDesc, IID_PPV_ARGS(&m_commandQueue)
	));
	ThrowIfFailed(device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator,
		nullptr,
		IID_PPV_ARGS(m_commandList.GetAddressOf())
	));

	m_commandList->Close();

	//create fence
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

	m_fenceEvent = fenceEvent;
	return;
}

void DX12CommandList::FlushCommandQueue()
{
	// Advance the fence value to mark commands up to this fence point.
	++m_fenceValue;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));

	// Wait until the GPU has completed commands up to this fence point.
	if (m_fence->GetCompletedValue() < m_fenceValue)
	{
		// Fire event when GPU hits current fence.  
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}
}

void DX12CommandList::ResetList(ID3D12CommandAllocator* commandAllocator)
{
	ThrowIfFailed(m_commandList->Reset(commandAllocator, nullptr));
	return;
}

void DX12CommandList::ResetList(ID3D12PipelineState* pInitiaState, ID3D12CommandAllocator* commandAllocator)
{
	ThrowIfFailed(m_commandList->Reset(commandAllocator, pInitiaState));
	return;
}

void DX12CommandList::ExecuteCommandLists(UINT numCommandLists, ID3D12CommandList** ppCommandLists)
{
	m_commandQueue->ExecuteCommandLists(numCommandLists, ppCommandLists);

	return;
}

//this function never be called except initialize or resize
void DX12CommandList::SubmitAndWait()
{
	m_commandList->Close();
	ID3D12CommandList* cmdsLists[] = { m_commandList.Get()};
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	FlushCommandQueue();
}

UINT64 DX12CommandList::Signal()
{
	++m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));
	return m_fenceValue;
}

//record must be done when use transitioned resource (draw/render/copy.. )
//in other situation, it would be better to stack transition command to flush toghter.
void DX12CommandList::RecordResourceStateTransition()
{
	if (m_resourceStateTransitionStack.empty())
	{
		::OutputDebugStringA("There is no Stacked Resource State Transition!");
		return;
	}

	UINT numTransitionCommands = SizeToU32(m_resourceStateTransitionStack.size());
	m_commandList->ResourceBarrier(numTransitionCommands, m_resourceStateTransitionStack.data());
	m_resourceStateTransitionStack.clear();
}