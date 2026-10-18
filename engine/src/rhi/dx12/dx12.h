#ifndef DX_12_H
#define DX_12_H

#include "rhi/swapchain_internal.h"
#include "rhi/image_internal.h"
#include "rhi/buffer_internal.h"
#include "rhi/sampler_internal.h"
#include "rhi/device_internal.h"
#include "rhi/instance_internal.h"
#include "rhi/pipeline.h"
#include "rhi/pipeline_internal.h"
#include "rhi/buffer.h"
#include "rhi/sampler.h"
#include "window/window.h"
#include <stdbool.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include "dx12_command_list_internal.h"

typedef struct DX12Instance {
    RHI_Instance_Impl base;
    IDXGIFactory6*     factory;
    bool                debug;
} DX12Instance;

typedef struct DX12Swapchain DX12Swapchain;
typedef struct DX12Image { 
    RHI_Image_Impl base;
    DX12Swapchain* owner;
    uint32_t index;
    ID3D12Resource* resource;
    ID3D12DescriptorHeap* rtvHeap;
    ID3D12DescriptorHeap* srvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;
    D3D12_RESOURCE_STATES state;
} DX12Image;
typedef struct DX12Buffer { 
    RHI_Buffer_Impl base; 
    ID3D12Resource* resource;
    D3D12_VERTEX_BUFFER_VIEW vertexView; 
} DX12Buffer;

typedef struct DX12Sampler { 
    RHI_Sampler_Impl base; 
    ID3D12DescriptorHeap* heap;
} DX12Sampler;

typedef struct DX12Device {
    RHI_Device_Impl     base;
    DX12Instance*        instance;
    IDXGIAdapter1*       adapter;
    ID3D12Device*         device;
    ID3D12CommandQueue*   graphicsQueue;
    ID3D12CommandQueue*   computeQueue;
    ID3D12CommandQueue*   transferQueue;

    DX12CommandList       graphicsCommandList;
    DX12CommandList       computeCommandList;
    DX12CommandList       transferCommandList;

    /* Synchronizes one-off uploads submitted to transferQueue (e.g. staging-buffer   */
    /* copies). Uploads are issued and waited on synchronously, so a single fence/    */
    /* event pair is enough - see DX12_Buffer_Copy in dx12_resources.c.               */
    ID3D12Fence*           transferFence;
    HANDLE                 transferFenceEvent;
    UINT64                 nextTransferFenceValue;

    DX12Swapchain*        boundSwapchain;
    const RHI_RenderPass* activeRenderPass;
} DX12Device;

typedef struct DX12Swapchain {
    RHI_Swapchain_Impl   base;
    DX12Device*           device;

    IDXGISwapChain4*       swapchain;
    HWND                   hwnd;

    ID3D12DescriptorHeap*  rtvHeap;
    UINT                    rtvDescriptorSize;

    ID3D12Resource*        backbuffers[DX12_FRAME_COUNT];
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[DX12_FRAME_COUNT];
    DX12Image images[DX12_FRAME_COUNT];
    RHI_Image imageHandles[DX12_FRAME_COUNT];

    uint32_t                width;
    uint32_t                height;
    uint32_t                currentBackbufferIndex;

    ID3D12Fence*            frameFence;
    HANDLE                  frameFenceEvent;
    UINT64                  nextFenceValue;
    UINT64                  backbufferFenceValues[DX12_FRAME_COUNT];
} DX12Swapchain;

typedef struct DX12PipelineLayout {
    RHI_PipelineLayout_Impl base;
    ID3D12RootSignature*     rootSignature;
} DX12PipelineLayout;

typedef struct DX12Pipeline {
    RHI_Pipeline_Impl   base;
    ID3D12PipelineState* pso;
    ID3D12RootSignature*  rootSignature;
    RHI_PrimitiveTopology topology;
    bool isCompute;
    uint32_t vertexStrides[RHI_PIPELINE_MAX_VERTEX_ATTRIBUTES];
} DX12Pipeline;

bool DX12_Instance_Create(bool debug, RHI_Instance_Impl** instance);
bool DX12_Device_Create(RHI_Instance instance, RHI_Device_Impl** device);
bool DX12_Swapchain_Create(RHI_Device device, Window* window, RHI_Swapchain_Impl** swapchain);
bool DX12_PipelineLayout_Create(RHI_Device device, const RHI_PipelineLayoutDesc* desc, RHI_PipelineLayout_Impl** outLayout);
bool DX12_Pipeline_Create(RHI_Device device, const RHI_PipelineDesc* desc, RHI_Pipeline_Impl** outPipeline);
bool DX12_Image_Create(RHI_Device device, const RHI_ImageDesc* desc, RHI_Image_Impl** out);
bool DX12_Buffer_Create(RHI_Device device, const RHI_BufferDesc* desc, RHI_Buffer_Impl** out);
bool DX12_Sampler_Create(RHI_Device device, const RHI_SamplerDesc* desc, RHI_Sampler_Impl** out);
bool DX12_Buffer_Create(RHI_Device device, const RHI_BufferDesc* desc, RHI_Buffer_Impl** out);
bool DX12_Image_Create(RHI_Device device, const RHI_ImageDesc* desc, RHI_Image_Impl** out);
DXGI_FORMAT DX12_ImageFormat_To_DXGI(RHI_ImageFormat format);


#endif