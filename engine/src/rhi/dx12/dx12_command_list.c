#include "dx12_command_list_internal.h"

bool DX12_CommandList_Init(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, DX12CommandList* out) {
    for (UINT i = 0; i < DX12_FRAME_COUNT; ++i) {
        HRESULT hr = device->lpVtbl->CreateCommandAllocator(
            device, type, &IID_ID3D12CommandAllocator, (void**)&out->allocators[i]);
        if (FAILED(hr)) {
            return false;
        }
    }

    HRESULT hr = device->lpVtbl->CreateCommandList(
        device, 0, type, out->allocators[0], NULL,
        &IID_ID3D12GraphicsCommandList, (void**)&out->list);
    if (FAILED(hr)) {
        return false;
    }

    out->list->lpVtbl->Close(out->list);
    return true;
}

void DX12_CommandList_Shutdown(DX12CommandList* cmdList) {
    if (cmdList->list) {
        cmdList->list->lpVtbl->Release(cmdList->list);
        cmdList->list = NULL;
    }
    for (UINT i = 0; i < DX12_FRAME_COUNT; ++i) {
        if (cmdList->allocators[i]) {
            cmdList->allocators[i]->lpVtbl->Release(cmdList->allocators[i]);
            cmdList->allocators[i] = NULL;
        }
    }
}

bool DX12_CommandList_Begin(DX12CommandList* cmdList, uint32_t frameIndex) {
    if (frameIndex >= DX12_FRAME_COUNT) {
        return false;
    }

    ID3D12CommandAllocator* allocator = cmdList->allocators[frameIndex];
    HRESULT hr = allocator->lpVtbl->Reset(allocator);
    if (FAILED(hr)) {
        return false;
    }

    hr = cmdList->list->lpVtbl->Reset(cmdList->list, allocator, NULL);
    return SUCCEEDED(hr);
}

void DX12_CommandList_End(DX12CommandList* cmdList, ID3D12CommandQueue* submitQueue) {
    cmdList->list->lpVtbl->Close(cmdList->list);

    ID3D12CommandList* lists[] = { (ID3D12CommandList*)cmdList->list };
    submitQueue->lpVtbl->ExecuteCommandLists(submitQueue, 1, lists);
}