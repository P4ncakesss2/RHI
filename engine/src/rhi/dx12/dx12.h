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
    D3D12_CPU_DESCRIPTOR_HANDLE uav;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;
    D3D12_RESOURCE_STATES state;
    bool storage;
    bool renderTarget;
    bool sampled;
} DX12Image;
typedef struct DX12Buffer { 
    RHI_Buffer_Impl base; 
    ID3D12Resource* resource;
    D3D12_VERTEX_BUFFER_VIEW vertexView; 
    D3D12_RESOURCE_STATES state;
} DX12Buffer;

typedef struct DX12Sampler { 
    RHI_Sampler_Impl base; 
    ID3D12DescriptorHeap* heap;
} DX12Sampler;

typedef struct {
    uint32_t binding;
    uint32_t rootParameterIndex;
    RHI_BindingType type;
} DX12BindingSlot;

typedef struct DX12PipelineLayout {
    RHI_PipelineLayout_Impl base;
    ID3D12RootSignature*     rootSignature;
    DX12BindingSlot           slots[RHI_PIPELINE_MAX_BINDINGS];
    uint32_t                  slotCount;
    uint32_t pushConstantParameter;
    uint32_t pushConstantSize;
} DX12PipelineLayout;

typedef struct DX12Pipeline {
    RHI_Pipeline_Impl   base;
    ID3D12PipelineState* pso;
    ID3D12RootSignature*  rootSignature;
    DX12PipelineLayout*   layout;
    RHI_PrimitiveTopology topology;
    bool isCompute;
    uint32_t vertexStrides[RHI_PIPELINE_MAX_VERTEX_ATTRIBUTES];
} DX12Pipeline;

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

    ID3D12Fence*           transferFence;
    HANDLE                 transferFenceEvent;
    UINT64                 nextTransferFenceValue;
    ID3D12Fence* frameFence;
    HANDLE frameFenceEvent;
    UINT64 nextFrameFenceValue;
    UINT64 frameFenceValues[DX12_FRAME_COUNT];
    uint32_t frameIndex;
    bool frameRecording;

    DX12Swapchain*        boundSwapchain;
    RHI_RenderPass activeRenderPass;
    bool hasActiveRenderPass;
    DX12Pipeline*          boundPipeline;
    ID3D12Resource* boundUavResources[RHI_PIPELINE_MAX_BINDINGS];
    bool pendingUavWrites[RHI_PIPELINE_MAX_BINDINGS];

    ID3D12DescriptorHeap*  srvHeap;
    UINT                    srvDescriptorSize;
    UINT                    srvHeapCapacity;
    UINT                    srvHeapCursor;

    ID3D12DescriptorHeap*  samplerHeap;
    UINT                    samplerDescriptorSize;
    UINT                    samplerHeapCapacity;
    UINT                    samplerHeapCursor;
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

bool DX12_Instance_Create(bool debug, RHI_Instance_Impl** instance);
bool DX12_Device_Create(RHI_Instance instance, RHI_Device_Impl** device);
bool DX12_Swapchain_Create(RHI_Device device, Window* window, RHI_Swapchain_Impl** swapchain);
bool DX12_PipelineLayout_Create(RHI_Device device, const RHI_PipelineLayoutDesc* desc, RHI_PipelineLayout_Impl** outLayout);
bool DX12_Pipeline_Create(RHI_Device device, const RHI_PipelineDesc* desc, RHI_Pipeline_Impl** outPipeline);
bool DX12_Image_Create(RHI_Device device, const RHI_ImageDesc* desc, RHI_Image_Impl** out);
bool DX12_Image_Resize(RHI_Device device, RHI_Image_Impl* image, uint32_t width, uint32_t height);
bool DX12_Buffer_Create(RHI_Device device, const RHI_BufferDesc* desc, RHI_Buffer_Impl** out);
bool DX12_Sampler_Create(RHI_Device device, const RHI_SamplerDesc* desc, RHI_Sampler_Impl** out);
bool DX12_Buffer_Create(RHI_Device device, const RHI_BufferDesc* desc, RHI_Buffer_Impl** out);
bool DX12_Image_Create(RHI_Device device, const RHI_ImageDesc* desc, RHI_Image_Impl** out);
DXGI_FORMAT DX12_ImageFormat_To_DXGI(RHI_ImageFormat format);
void DX12_DumpDebugMessages(ID3D12Device* device);

#endif
