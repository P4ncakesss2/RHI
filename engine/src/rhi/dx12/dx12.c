#include "dx12.h"
#include "rhi/device_internal.h"
#include "rhi/instance_internal.h"
#include "rhi/image_internal.h"
#include <stdlib.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>

static void DX12_Instance_Destroy(RHI_Instance_Impl* instance) {
    DX12Instance* dxInstance = (DX12Instance*)instance;
    if (dxInstance->factory) {
        dxInstance->factory->lpVtbl->Release(dxInstance->factory);
    }
    free(dxInstance);
}

static bool DX12_Device_BeginFrame(RHI_Device_Impl* self, uint32_t frameIndex) {
    DX12Device* dxDevice = (DX12Device*)self;
    return DX12_CommandList_Begin(&dxDevice->graphicsCommandList, frameIndex);
}

static void DX12_Device_EndFrame(RHI_Device_Impl* self) {
    DX12Device* dxDevice = (DX12Device*)self;

    if (dxDevice->boundSwapchain) {
        DX12Swapchain* dxSwapchain = dxDevice->boundSwapchain;
        ID3D12GraphicsCommandList* list = dxDevice->graphicsCommandList.list;
        ID3D12Resource* backbuffer = dxSwapchain->backbuffers[dxSwapchain->currentBackbufferIndex];

        D3D12_RESOURCE_BARRIER toPresent = {0};
        toPresent.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toPresent.Transition.pResource = backbuffer;
        toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        toPresent.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        list->lpVtbl->ResourceBarrier(list, 1, &toPresent);

        dxDevice->boundSwapchain = NULL;
    }

    DX12_CommandList_End(&dxDevice->graphicsCommandList, dxDevice->graphicsQueue);
}

static void DX12_Device_Destroy(RHI_Device_Impl* device) {
    DX12Device* dxDevice = (DX12Device*)device;

    DX12_CommandList_Shutdown(&dxDevice->graphicsCommandList);
    DX12_CommandList_Shutdown(&dxDevice->computeCommandList);
    DX12_CommandList_Shutdown(&dxDevice->transferCommandList);

    if (dxDevice->transferFenceEvent) {
        CloseHandle(dxDevice->transferFenceEvent);
        dxDevice->transferFenceEvent = NULL;
    }
    if (dxDevice->transferFence) {
        dxDevice->transferFence->lpVtbl->Release(dxDevice->transferFence);
        dxDevice->transferFence = NULL;
    }

    if (dxDevice->transferQueue) dxDevice->transferQueue->lpVtbl->Release(dxDevice->transferQueue);
    if (dxDevice->computeQueue)  dxDevice->computeQueue->lpVtbl->Release(dxDevice->computeQueue);
    if (dxDevice->graphicsQueue) dxDevice->graphicsQueue->lpVtbl->Release(dxDevice->graphicsQueue);
    if (dxDevice->device)        dxDevice->device->lpVtbl->Release(dxDevice->device);
    if (dxDevice->adapter)       dxDevice->adapter->lpVtbl->Release(dxDevice->adapter);

    free(dxDevice);
}

static bool DX12_Device_BeginRenderPass(RHI_Device_Impl *self, const RHI_RenderPass *pass) {
    DX12Device *d = (DX12Device *)self;
    RHI_Image_Impl **ip = RHI_Image_Get_Impl(pass->colorTarget);
    if (!ip || !*ip) {
        return false;
    }
    
    DX12Image *image = (DX12Image *)*ip;
    DX12Swapchain *sc = image->owner;
    ID3D12GraphicsCommandList *list = d->graphicsCommandList.list;
    ID3D12Resource *resource = image->resource;
    
    D3D12_RESOURCE_BARRIER b = {0};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = resource;
    b.Transition.StateBefore = sc ? D3D12_RESOURCE_STATE_PRESENT : image->state;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->lpVtbl->ResourceBarrier(list, 1, &b);
    
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = sc ? sc->rtvHandles[image->index] : image->rtv;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = {0};
    D3D12_CPU_DESCRIPTOR_HANDLE *dsvPtr = NULL;
    DX12Image *depthImage = NULL;
    
    if (pass->hasDepthTarget) {
        RHI_Image_Impl **dp = RHI_Image_Get_Impl(pass->depthTarget);
        if (!dp || !*dp) {
            return false;
        }
        
        depthImage = (DX12Image *)*dp;
        if (depthImage->state != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
            D3D12_RESOURCE_BARRIER db = {0};
            db.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            db.Transition.pResource = depthImage->resource;
            db.Transition.StateBefore = depthImage->state;
            db.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            db.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            list->lpVtbl->ResourceBarrier(list, 1, &db);
            depthImage->state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        }
        dsv = depthImage->dsv;
        dsvPtr = &dsv;
    }
    
    list->lpVtbl->OMSetRenderTargets(list, 1, &rtv, FALSE, dsvPtr);
    
    if (pass->clearColor) {
        FLOAT c[4] = { pass->colorClear.r, pass->colorClear.g, pass->colorClear.b, pass->colorClear.a };
        list->lpVtbl->ClearRenderTargetView(list, rtv, c, 0, NULL);
    }
    
    if (depthImage && (pass->clearDepth || pass->clearStencil)) {
        D3D12_CLEAR_FLAGS flags = (pass->clearDepth ? D3D12_CLEAR_FLAG_DEPTH : 0) |
                                  (pass->clearStencil ? D3D12_CLEAR_FLAG_STENCIL : 0);
        list->lpVtbl->ClearDepthStencilView(list, dsv, flags, pass->depthClear, pass->stencilClear, 0, NULL);
    }
    
    D3D12_VIEWPORT vp = {
        pass->viewport.x,
        pass->viewport.y,
        pass->viewport.width,
        pass->viewport.height,
        pass->viewport.minDepth,
        pass->viewport.maxDepth
    };
    
    D3D12_RECT sr = {
        pass->scissor.x,
        pass->scissor.y,
        pass->scissor.x + (LONG)pass->scissor.width,
        pass->scissor.y + (LONG)pass->scissor.height
    };
    
    list->lpVtbl->RSSetViewports(list, 1, &vp);
    list->lpVtbl->RSSetScissorRects(list, 1, &sr);
    
    if (!sc) {
        image->state = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    
    d->boundSwapchain = sc;
    d->activeRenderPass = pass;
    return true;
}

static void DX12_Device_EndRenderPass(RHI_Device_Impl *self) {
    ((DX12Device *)self)->activeRenderPass = NULL;
}

static void DX12_Device_BindPipeline(RHI_Device_Impl *self, RHI_Pipeline_Impl *pipelineImpl) {
    DX12Device *dxDevice = (DX12Device *)self;
    DX12Pipeline *dxPipeline = (DX12Pipeline *)pipelineImpl;

    ID3D12GraphicsCommandList *list = dxDevice->graphicsCommandList.list;
    list->lpVtbl->SetPipelineState(list, dxPipeline->pso);
    
    if (dxPipeline->isCompute) {
        list->lpVtbl->SetComputeRootSignature(list, dxPipeline->rootSignature);
        return;
    }
    
    list->lpVtbl->SetGraphicsRootSignature(list, dxPipeline->rootSignature);
    if (!dxDevice->activeRenderPass) {
        return;
    }
    
    D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    switch (dxPipeline->topology) {
        case RHI_TOPOLOGY_TRIANGLE_STRIP:
            topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            break;
        case RHI_TOPOLOGY_LINE_LIST:
            topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            break;
        case RHI_TOPOLOGY_LINE_STRIP:
            topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
            break;
        case RHI_TOPOLOGY_POINT_LIST:
            topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            break;
        default:
            break;
    }
    list->lpVtbl->IASetPrimitiveTopology(list, topology);
}

static void DX12_Device_Draw(RHI_Device_Impl *self, uint32_t vertexCount, uint32_t instanceCount,
                             uint32_t firstVertex, uint32_t firstInstance) {
    DX12Device *dxDevice = (DX12Device *)self;
    ID3D12GraphicsCommandList *list = dxDevice->graphicsCommandList.list;
    list->lpVtbl->DrawInstanced(list, vertexCount, instanceCount, firstVertex, firstInstance);
}

static void DX12_Device_Dispatch(RHI_Device_Impl *self, uint32_t x, uint32_t y, uint32_t z) {
    DX12Device *dxDevice = (DX12Device *)self;
    dxDevice->graphicsCommandList.list->lpVtbl->Dispatch(dxDevice->graphicsCommandList.list, x, y, z);
}

static bool DX12_Device_BindVertexBuffer(RHI_Device_Impl *self, uint32_t slot, RHI_Buffer_Impl *impl, uint32_t stride) {
    if (slot >= D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT || !(impl->usage & RHI_BUFFER_VERTEX) || !stride) {
        return false;
    }
    
    DX12Device *d = (DX12Device *)self;
    DX12Buffer *b = (DX12Buffer *)impl;
    D3D12_VERTEX_BUFFER_VIEW v = b->vertexView;
    v.StrideInBytes = stride;
    
    d->graphicsCommandList.list->lpVtbl->IASetVertexBuffers(d->graphicsCommandList.list, slot, 1, &v);
    return true;
}

static bool DX12_Device_BindIndexBuffer(RHI_Device_Impl *self, RHI_Buffer_Impl *impl, bool index32) {
    if (!(impl->usage & RHI_BUFFER_INDEX)) {
        return false;
    }
    
    DX12Device *d = (DX12Device *)self;
    DX12Buffer *b = (DX12Buffer *)impl;
    D3D12_INDEX_BUFFER_VIEW v = {0};
    v.BufferLocation = b->resource->lpVtbl->GetGPUVirtualAddress(b->resource);
    v.SizeInBytes = (UINT)impl->size;
    v.Format = index32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    
    d->graphicsCommandList.list->lpVtbl->IASetIndexBuffer(d->graphicsCommandList.list, &v);
    return true;
}

static void DX12_Device_DrawIndexed(RHI_Device_Impl *self, uint32_t n, uint32_t instances, uint32_t first, int32_t offset, uint32_t firstInstance) {
    DX12Device *d = (DX12Device *)self;
    d->graphicsCommandList.list->lpVtbl->DrawIndexedInstanced(d->graphicsCommandList.list, n, instances, first, offset, firstInstance);
}

static const RHI_Device_Vtable dx12DeviceVtable = {
    .destroy = DX12_Device_Destroy,
    .beginFrame = DX12_Device_BeginFrame,
    .endFrame = DX12_Device_EndFrame,
    .beginRenderPass = DX12_Device_BeginRenderPass,
    .endRenderPass = DX12_Device_EndRenderPass,
    .bindPipeline = DX12_Device_BindPipeline,
    .draw = DX12_Device_Draw,
    .dispatch = DX12_Device_Dispatch,
    .bindVertexBuffer = DX12_Device_BindVertexBuffer,
    .bindIndexBuffer = DX12_Device_BindIndexBuffer,
    .drawIndexed = DX12_Device_DrawIndexed,
};

static const RHI_Instance_Vtable dx12InstanceVtable = {
    .destroy = DX12_Instance_Destroy
};

bool DX12_Instance_Create(bool debug, RHI_Instance_Impl** instance) {
    DX12Instance* dxInstance = calloc(1, sizeof(DX12Instance));
    if (!dxInstance) {
        return false;
    }

    dxInstance->base.vtable = &dx12InstanceVtable;
    dxInstance->debug = debug;

    UINT factoryFlags = 0;

    if (debug) {
        ID3D12Debug* debugController = NULL;
        if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (void**)&debugController))) {
            debugController->lpVtbl->EnableDebugLayer(debugController);
            debugController->lpVtbl->Release(debugController);
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }

    HRESULT hr = CreateDXGIFactory2(factoryFlags, &IID_IDXGIFactory6, (void**)&dxInstance->factory);
    if (FAILED(hr)) {
        free(dxInstance);
        return false;
    }

    *instance = &dxInstance->base;
    return true;
}

static bool DX12_PickAdapter(DX12Instance* dxInstance, IDXGIAdapter1** outAdapter) {
    IDXGIAdapter1* adapter = NULL;

    for (UINT i = 0;
         dxInstance->factory->lpVtbl->EnumAdapterByGpuPreference(
             dxInstance->factory, i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
             &IID_IDXGIAdapter1, (void**)&adapter) != DXGI_ERROR_NOT_FOUND;
         ++i) {

        DXGI_ADAPTER_DESC1 desc;
        adapter->lpVtbl->GetDesc1(adapter, &desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            adapter->lpVtbl->Release(adapter);
            adapter = NULL;
            continue; // skip the WARP/software adapter
        }

        if (SUCCEEDED(D3D12CreateDevice((IUnknown*)adapter, D3D_FEATURE_LEVEL_11_0,
                                          &IID_ID3D12Device, NULL))) {
            *outAdapter = adapter;
            return true;
        }

        adapter->lpVtbl->Release(adapter);
        adapter = NULL;
    }

    return false;
}

static bool DX12_CreateQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type,
                              ID3D12CommandQueue** outQueue) {
    D3D12_COMMAND_QUEUE_DESC desc = {0};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    HRESULT hr = device->lpVtbl->CreateCommandQueue(device, &desc, &IID_ID3D12CommandQueue,
                                                       (void**)outQueue);
    return SUCCEEDED(hr);
}

bool DX12_Device_Create(RHI_Instance instance, RHI_Device_Impl** device) {
    RHI_Instance_Impl** instance_impl = RHI_Instance_Get_Impl(instance);
    if (!instance_impl || !*instance_impl) {
        return false;
    }
    DX12Instance* dxInstance = (DX12Instance*)*instance_impl;

    DX12Device* dxDevice = calloc(1, sizeof(DX12Device));
    if (!dxDevice) {
        return false;
    }

    dxDevice->base.vtable = &dx12DeviceVtable;
    dxDevice->instance = dxInstance;

    if (!DX12_PickAdapter(dxInstance, &dxDevice->adapter)) {
        free(dxDevice);
        return false;
    }

    HRESULT hr = D3D12CreateDevice((IUnknown*)dxDevice->adapter, D3D_FEATURE_LEVEL_11_0,
                                     &IID_ID3D12Device, (void**)&dxDevice->device);
    if (FAILED(hr)) {
        dxDevice->adapter->lpVtbl->Release(dxDevice->adapter);
        free(dxDevice);
        return false;
    }

    bool ok = true;
    ok &= DX12_CreateQueue(dxDevice->device, D3D12_COMMAND_LIST_TYPE_DIRECT,  &dxDevice->graphicsQueue);
    ok &= DX12_CreateQueue(dxDevice->device, D3D12_COMMAND_LIST_TYPE_COMPUTE, &dxDevice->computeQueue);
    ok &= DX12_CreateQueue(dxDevice->device, D3D12_COMMAND_LIST_TYPE_COPY,    &dxDevice->transferQueue);
    ok &= DX12_CommandList_Init(dxDevice->device, D3D12_COMMAND_LIST_TYPE_DIRECT,  &dxDevice->graphicsCommandList);
    ok &= DX12_CommandList_Init(dxDevice->device, D3D12_COMMAND_LIST_TYPE_COMPUTE, &dxDevice->computeCommandList);
    ok &= DX12_CommandList_Init(dxDevice->device, D3D12_COMMAND_LIST_TYPE_COPY,    &dxDevice->transferCommandList);

    if (ok) {
        HRESULT fenceHr = dxDevice->device->lpVtbl->CreateFence(
            dxDevice->device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&dxDevice->transferFence);
        ok &= SUCCEEDED(fenceHr);
    }
    if (ok) {
        dxDevice->transferFenceEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
        ok &= dxDevice->transferFenceEvent != NULL;
    }
    dxDevice->nextTransferFenceValue = 1;

    if (!ok) {
        DX12_Device_Destroy(&dxDevice->base); // unwinds whatever partially succeeded
        return false;
    }

    *device = &dxDevice->base;
    return true;
}