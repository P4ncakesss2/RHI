#ifndef DX12_COMMAND_LIST_INTERNAL_H
#define DX12_COMMAND_LIST_INTERNAL_H

#include <d3d12.h>
#include <stdint.h>
#include <stdbool.h>

#define DX12_FRAME_COUNT 2

typedef struct {
    ID3D12CommandAllocator*    allocators[DX12_FRAME_COUNT];
    ID3D12GraphicsCommandList* list;
} DX12CommandList;

bool DX12_CommandList_Init(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, DX12CommandList* out);
void DX12_CommandList_Shutdown(DX12CommandList* cmdList);

bool DX12_CommandList_Begin(DX12CommandList* cmdList, uint32_t frameIndex);
void DX12_CommandList_End(DX12CommandList* cmdList, ID3D12CommandQueue* submitQueue);

#endif