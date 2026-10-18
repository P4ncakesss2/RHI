#include "dx12.h"
#include "rhi/device_internal.h"
#include "rhi/instance_internal.h"
#include "rhi/image_internal.h"
#include <stdlib.h>
#include <string.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <stdio.h>

#define DX12_SRV_HEAP_CAPACITY     1024
#define DX12_SAMPLER_HEAP_CAPACITY 256

#define DX12_CHECK(hr, device, msg) \
    do { if (FAILED(hr)) { fprintf(stderr, "%s: hr=0x%08lX\n", msg, (unsigned long)(hr)); DX12_DumpDebugMessages(device); } } while (0)

void DX12_DumpDebugMessages(ID3D12Device* device) {
    if (!device) return;
    ID3D12InfoQueue* infoQueue = NULL;
    if (FAILED(device->lpVtbl->QueryInterface(device, &IID_ID3D12InfoQueue, (void**)&infoQueue)) || !infoQueue) {
        return;
    }

    UINT64 count = infoQueue->lpVtbl->GetNumStoredMessages(infoQueue);
    for (UINT64 i = 0; i < count; ++i) {
        SIZE_T msgLen = 0;
        infoQueue->lpVtbl->GetMessage(infoQueue, i, NULL, &msgLen);
        if (msgLen == 0) continue;

        D3D12_MESSAGE* msg = (D3D12_MESSAGE*)malloc(msgLen);
        if (!msg) continue;

        if (SUCCEEDED(infoQueue->lpVtbl->GetMessage(infoQueue, i, msg, &msgLen))) {
            fprintf(stderr, "[D3D12] %s\n", msg->pDescription);
        }
        free(msg);
    }
    infoQueue->lpVtbl->ClearStoredMessages(infoQueue);
    infoQueue->lpVtbl->Release(infoQueue);
}

static void DX12_Instance_Destroy(RHI_Instance_Impl* instance) {
    DX12Instance* dxInstance = (DX12Instance*)instance;
    if (dxInstance->factory) {
        dxInstance->factory->lpVtbl->Release(dxInstance->factory);
    }
    free(dxInstance);
}

static bool DX12_Device_BeginFrame(RHI_Device_Impl* self, uint32_t frameIndex) {
    DX12Device* dxDevice = (DX12Device*)self;
    if (frameIndex >= DX12_FRAME_COUNT) return false;
    UINT64 frameFenceValue = dxDevice->frameFenceValues[frameIndex];
    if (frameFenceValue && dxDevice->frameFence->lpVtbl->GetCompletedValue(dxDevice->frameFence) < frameFenceValue) {
        dxDevice->frameFence->lpVtbl->SetEventOnCompletion(dxDevice->frameFence, frameFenceValue, dxDevice->frameFenceEvent);
        WaitForSingleObject(dxDevice->frameFenceEvent, INFINITE);
    }
    dxDevice->frameIndex = frameIndex;

    if (!DX12_CommandList_Begin(&dxDevice->graphicsCommandList, frameIndex)) {
        return false;
    }
    dxDevice->frameRecording = true;

    dxDevice->srvHeapCursor = 0;
    dxDevice->samplerHeapCursor = 0;
    dxDevice->boundPipeline = NULL;

    ID3D12DescriptorHeap* heaps[] = { dxDevice->srvHeap, dxDevice->samplerHeap };
    dxDevice->graphicsCommandList.list->lpVtbl->SetDescriptorHeaps(
        dxDevice->graphicsCommandList.list, 2, heaps);

    return true;
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
    dxDevice->frameRecording = false;
    UINT64 frameFenceValue = dxDevice->nextFrameFenceValue++;
    if (SUCCEEDED(dxDevice->graphicsQueue->lpVtbl->Signal(dxDevice->graphicsQueue, dxDevice->frameFence, frameFenceValue))) {
        dxDevice->frameFenceValues[dxDevice->frameIndex] = frameFenceValue;
    }
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
    if (dxDevice->frameFenceEvent) CloseHandle(dxDevice->frameFenceEvent);
    if (dxDevice->frameFence) dxDevice->frameFence->lpVtbl->Release(dxDevice->frameFence);

    if (dxDevice->srvHeap) dxDevice->srvHeap->lpVtbl->Release(dxDevice->srvHeap);
    if (dxDevice->samplerHeap) dxDevice->samplerHeap->lpVtbl->Release(dxDevice->samplerHeap);

    if (dxDevice->transferQueue) dxDevice->transferQueue->lpVtbl->Release(dxDevice->transferQueue);
    if (dxDevice->computeQueue)  dxDevice->computeQueue->lpVtbl->Release(dxDevice->computeQueue);
    if (dxDevice->graphicsQueue) dxDevice->graphicsQueue->lpVtbl->Release(dxDevice->graphicsQueue);
    if (dxDevice->device)        dxDevice->device->lpVtbl->Release(dxDevice->device);
    if (dxDevice->adapter)       dxDevice->adapter->lpVtbl->Release(dxDevice->adapter);

    free(dxDevice);
}

static void DX12_Transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource,
                            D3D12_RESOURCE_STATES* state, D3D12_RESOURCE_STATES target);

static bool DX12_Device_BeginRenderPass(RHI_Device_Impl *self, const RHI_RenderPass *pass) {
    DX12Device *d = (DX12Device *)self;
    uint32_t colorCount = pass->colorAttachmentCount ? pass->colorAttachmentCount : 1;
    if (colorCount > RHI_PIPELINE_MAX_COLOR_ATTACHMENTS) return false;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[RHI_PIPELINE_MAX_COLOR_ATTACHMENTS];
    DX12Image* images[RHI_PIPELINE_MAX_COLOR_ATTACHMENTS] = {0};
    DX12Swapchain* sc = NULL;
    ID3D12GraphicsCommandList *list = d->graphicsCommandList.list;
    if (pass->resolveAttachmentCount > colorCount) return false;
    for (uint32_t i = 0; i < colorCount; ++i) {
        RHI_Image colorHandle = pass->colorAttachmentCount ? pass->colorTargets[i] : pass->colorTarget;
        RHI_Image_Impl **ip = RHI_Image_Get_Impl(colorHandle);
        if (!ip || !*ip) return false;
        images[i] = (DX12Image*)*ip;
        DX12Swapchain* owner = images[i]->owner;
        if (owner) {
            if (sc && owner != sc) return false;
            sc = owner;
        }
        if (owner) images[i]->state = D3D12_RESOURCE_STATE_PRESENT;
        DX12_Transition(list, images[i]->resource, &images[i]->state, D3D12_RESOURCE_STATE_RENDER_TARGET);
        rtvs[i] = owner ? owner->rtvHandles[images[i]->index] : images[i]->rtv;
    }
    for (uint32_t i = 0; i < pass->resolveAttachmentCount; ++i) {
        RHI_Image_Impl** resolveImpl = RHI_Image_Get_Impl(pass->resolveTargets[i]);
        if (!resolveImpl || !*resolveImpl) return false;
        DX12Image* resolve = (DX12Image*)*resolveImpl;
        if (images[i]->base.info.sampleCount <= 1 || resolve->base.info.sampleCount != 1 ||
            images[i]->base.info.width != resolve->base.info.width ||
            images[i]->base.info.height != resolve->base.info.height ||
            images[i]->base.info.format != resolve->base.info.format) return false;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = {0};
    D3D12_CPU_DESCRIPTOR_HANDLE *dsvPtr = NULL;
    DX12Image *depthImage = NULL;
    
    if (pass->hasDepthTarget || pass->hasStencilTarget) {
        if (pass->hasDepthTarget && pass->hasStencilTarget &&
            !RHI_Image_Equal(pass->depthTarget, pass->stencilTarget)) return false;
        RHI_Image stencilHandle = pass->hasDepthTarget ? pass->depthTarget : pass->stencilTarget;
        RHI_Image_Impl **dp = RHI_Image_Get_Impl(stencilHandle);
        if (!dp || !*dp) {
            return false;
        }
        
        depthImage = (DX12Image *)*dp;
        if (pass->hasStencilTarget && depthImage->base.info.format != RHI_IMAGE_FORMAT_D24S8 &&
            depthImage->base.info.format != RHI_IMAGE_FORMAT_D32S8) return false;
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
    
    list->lpVtbl->OMSetRenderTargets(list, colorCount, rtvs, FALSE, dsvPtr);
    
    if (pass->clearColor) {
        FLOAT c[4] = { pass->colorClear.r, pass->colorClear.g, pass->colorClear.b, pass->colorClear.a };
        for (uint32_t i = 0; i < colorCount; ++i) list->lpVtbl->ClearRenderTargetView(list, rtvs[i], c, 0, NULL);
    }
    
    if (depthImage && (pass->clearDepth || pass->clearStencil)) {
        bool hasStencil = pass->hasStencilTarget || depthImage->base.info.format == RHI_IMAGE_FORMAT_D24S8 ||
                          depthImage->base.info.format == RHI_IMAGE_FORMAT_D32S8;
        D3D12_CLEAR_FLAGS flags = ((pass->clearDepth && pass->hasDepthTarget) ? D3D12_CLEAR_FLAG_DEPTH : 0) |
                                  ((pass->clearStencil && hasStencil) ? D3D12_CLEAR_FLAG_STENCIL : 0);
        if (flags) list->lpVtbl->ClearDepthStencilView(list, dsv, flags, pass->depthClear, pass->stencilClear, 0, NULL);
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
    
    
    d->boundSwapchain = sc;
    d->activeRenderPass = *pass;
    d->hasActiveRenderPass = true;
    return true;
}

static void DX12_Device_EndRenderPass(RHI_Device_Impl *self) {
    DX12Device* d = (DX12Device*)self;
    if (!d->hasActiveRenderPass) return;
    ID3D12GraphicsCommandList* list = d->graphicsCommandList.list;
    for (uint32_t i = 0; i < d->activeRenderPass.resolveAttachmentCount; ++i) {
        RHI_Image colorHandle = d->activeRenderPass.colorAttachmentCount
            ? d->activeRenderPass.colorTargets[i]
            : d->activeRenderPass.colorTarget;
        RHI_Image_Impl** sourceImpl = RHI_Image_Get_Impl(colorHandle);
        RHI_Image_Impl** destinationImpl = RHI_Image_Get_Impl(d->activeRenderPass.resolveTargets[i]);
        if (!sourceImpl || !*sourceImpl || !destinationImpl || !*destinationImpl) continue;
        DX12Image* source = (DX12Image*)*sourceImpl;
        DX12Image* destination = (DX12Image*)*destinationImpl;
        DX12_Transition(list, source->resource, &source->state, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        DX12_Transition(list, destination->resource, &destination->state, D3D12_RESOURCE_STATE_RESOLVE_DEST);
        list->lpVtbl->ResolveSubresource(list, destination->resource, 0, source->resource, 0,
                                         DX12_ImageFormat_To_DXGI(source->base.info.format));
    }
    d->hasActiveRenderPass = false;
}

static void DX12_Device_FlushUavWrites(DX12Device* device) {
    ID3D12GraphicsCommandList* list = device->graphicsCommandList.list;
    for (uint32_t i = 0; i < RHI_PIPELINE_MAX_BINDINGS; ++i) {
        if (!device->pendingUavWrites[i] || !device->boundUavResources[i]) continue;
        D3D12_RESOURCE_BARRIER barrier = {0};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = device->boundUavResources[i];
        list->lpVtbl->ResourceBarrier(list, 1, &barrier);
        device->pendingUavWrites[i] = false;
    }
}

static void DX12_Device_MarkUavWrites(DX12Device* device) {
    for (uint32_t i = 0; i < RHI_PIPELINE_MAX_BINDINGS; ++i) {
        if (device->boundUavResources[i]) device->pendingUavWrites[i] = true;
    }
}

static void DX12_Device_BindPipeline(RHI_Device_Impl *self, RHI_Pipeline_Impl *pipelineImpl) {
    DX12Device *dxDevice = (DX12Device *)self;
    DX12_Device_FlushUavWrites(dxDevice);
    memset(dxDevice->boundUavResources, 0, sizeof(dxDevice->boundUavResources));
    memset(dxDevice->pendingUavWrites, 0, sizeof(dxDevice->pendingUavWrites));
    DX12Pipeline *dxPipeline = (DX12Pipeline *)pipelineImpl;

    dxDevice->boundPipeline = dxPipeline;

    ID3D12GraphicsCommandList *list = dxDevice->graphicsCommandList.list;
    list->lpVtbl->SetPipelineState(list, dxPipeline->pso);
    
    if (dxPipeline->isCompute) {
        list->lpVtbl->SetComputeRootSignature(list, dxPipeline->rootSignature);
        return;
    }
    
    list->lpVtbl->SetGraphicsRootSignature(list, dxPipeline->rootSignature);
    if (!dxDevice->hasActiveRenderPass) {
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
    DX12_Device_FlushUavWrites(dxDevice);
    ID3D12GraphicsCommandList *list = dxDevice->graphicsCommandList.list;
    list->lpVtbl->DrawInstanced(list, vertexCount, instanceCount, firstVertex, firstInstance);
    DX12_Device_MarkUavWrites(dxDevice);
}

static void DX12_Device_Dispatch(RHI_Device_Impl *self, uint32_t x, uint32_t y, uint32_t z) {
    DX12Device *dxDevice = (DX12Device *)self;
    DX12_Device_FlushUavWrites(dxDevice);
    dxDevice->graphicsCommandList.list->lpVtbl->Dispatch(dxDevice->graphicsCommandList.list, x, y, z);
    DX12_Device_MarkUavWrites(dxDevice);
}

static bool DX12_Device_BindVertexBuffer(RHI_Device_Impl *self, uint32_t slot, RHI_Buffer_Impl *impl, uint32_t stride) {
    if (slot >= D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT || !(impl->usage & RHI_BUFFER_VERTEX) || !stride) {
        return false;
    }
    
    DX12Device *d = (DX12Device *)self;
    DX12Buffer *b = (DX12Buffer *)impl;
    DX12_Transition(d->graphicsCommandList.list, b->resource, &b->state, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
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
    DX12_Transition(d->graphicsCommandList.list, b->resource, &b->state, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    D3D12_INDEX_BUFFER_VIEW v = {0};
    v.BufferLocation = b->resource->lpVtbl->GetGPUVirtualAddress(b->resource);
    v.SizeInBytes = (UINT)impl->size;
    v.Format = index32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    
    d->graphicsCommandList.list->lpVtbl->IASetIndexBuffer(d->graphicsCommandList.list, &v);
    return true;
}

static void DX12_Device_DrawIndexed(RHI_Device_Impl *self, uint32_t n, uint32_t instances, uint32_t first, int32_t offset, uint32_t firstInstance) {
    DX12Device *d = (DX12Device *)self;
    DX12_Device_FlushUavWrites(d);
    d->graphicsCommandList.list->lpVtbl->DrawIndexedInstanced(d->graphicsCommandList.list, n, instances, first, offset, firstInstance);
    DX12_Device_MarkUavWrites(d);
}

static const DX12BindingSlot* DX12_FindBindingSlot(DX12PipelineLayout* layout, uint32_t binding, RHI_BindingType type) {
    if (!layout) return NULL;
    for (uint32_t i = 0; i < layout->slotCount; ++i) {
        if (layout->slots[i].binding == binding && layout->slots[i].type == type) {
            return &layout->slots[i];
        }
    }
    return NULL;
}

static bool DX12_Device_BindBuffer(RHI_Device_Impl* self, uint32_t binding, RHI_Buffer_Impl* bufImpl) {
    DX12Device* d = (DX12Device*)self;
    if (!d->boundPipeline || !bufImpl) return false;

    DX12Buffer* buf = (DX12Buffer*)bufImpl;
    ID3D12GraphicsCommandList* list = d->graphicsCommandList.list;
    bool isCompute = d->boundPipeline->isCompute;

    const DX12BindingSlot* slot = DX12_FindBindingSlot(d->boundPipeline->layout, binding, RHI_BINDING_UNIFORM_BUFFER);
    bool isUniform = slot != NULL;
    if (!slot) {
        slot = DX12_FindBindingSlot(d->boundPipeline->layout, binding, RHI_BINDING_STORAGE_BUFFER);
    }
    if (!slot) slot = DX12_FindBindingSlot(d->boundPipeline->layout, binding, RHI_BINDING_STORAGE_BUFFER_UAV);
    if (!slot) return false;

    if (isUniform && !(bufImpl->usage & RHI_BUFFER_UNIFORM)) return false;
    if (!isUniform && !(bufImpl->usage & RHI_BUFFER_STORAGE)) return false;

    D3D12_RESOURCE_STATES desiredState = isUniform || slot->type == RHI_BINDING_STORAGE_BUFFER
        ? (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
        : D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    if (isUniform) desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    DX12_Device_FlushUavWrites(d);
    DX12_Transition(list, buf->resource, &buf->state, desiredState);
    d->boundUavResources[slot->rootParameterIndex] = slot->type == RHI_BINDING_STORAGE_BUFFER_UAV ? buf->resource : NULL;
    d->pendingUavWrites[slot->rootParameterIndex] = false;

    D3D12_GPU_VIRTUAL_ADDRESS addr = buf->resource->lpVtbl->GetGPUVirtualAddress(buf->resource);
    if (isCompute) {
        if (isUniform) {
            list->lpVtbl->SetComputeRootConstantBufferView(list, slot->rootParameterIndex, addr);
        } else if (slot->type == RHI_BINDING_STORAGE_BUFFER_UAV) {
            list->lpVtbl->SetComputeRootUnorderedAccessView(list, slot->rootParameterIndex, addr);
        } else {
            list->lpVtbl->SetComputeRootShaderResourceView(list, slot->rootParameterIndex, addr);
        }
    } else {
        if (isUniform) {
            list->lpVtbl->SetGraphicsRootConstantBufferView(list, slot->rootParameterIndex, addr);
        } else if (slot->type == RHI_BINDING_STORAGE_BUFFER_UAV) {
            list->lpVtbl->SetGraphicsRootUnorderedAccessView(list, slot->rootParameterIndex, addr);
        } else {
            list->lpVtbl->SetGraphicsRootShaderResourceView(list, slot->rootParameterIndex, addr);
        }
    }
    return true;
}

static bool DX12_Device_BindImage(RHI_Device_Impl* self, uint32_t binding, RHI_Image_Impl* imgImpl) {
    DX12Device* d = (DX12Device*)self;
    if (!d->boundPipeline || !imgImpl) return false;

    const DX12BindingSlot* slot = DX12_FindBindingSlot(d->boundPipeline->layout, binding, RHI_BINDING_SAMPLED_IMAGE);
    bool isStorage = false;
    if (!slot) { slot = DX12_FindBindingSlot(d->boundPipeline->layout, binding, RHI_BINDING_STORAGE_IMAGE); isStorage = true; }
    if (!slot) return false;

    DX12Image* img = (DX12Image*)imgImpl;
    if (!img->srvHeap || d->srvHeapCursor >= d->srvHeapCapacity || (isStorage && !img->storage)) return false;
    DX12_Device_FlushUavWrites(d);
    DX12_Transition(d->graphicsCommandList.list, img->resource, &img->state,
                    isStorage ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS :
                    (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
    d->boundUavResources[slot->rootParameterIndex] = isStorage ? img->resource : NULL;
    d->pendingUavWrites[slot->rootParameterIndex] = false;

    D3D12_CPU_DESCRIPTOR_HANDLE src;
    img->srvHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(img->srvHeap, &src);
    if (isStorage) src = img->uav;

    D3D12_CPU_DESCRIPTOR_HANDLE dst;
    d->srvHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(d->srvHeap, &dst);
    dst.ptr += (SIZE_T)d->srvHeapCursor * d->srvDescriptorSize;

    d->device->lpVtbl->CopyDescriptorsSimple(d->device, 1, dst, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu;
    d->srvHeap->lpVtbl->GetGPUDescriptorHandleForHeapStart(d->srvHeap, &gpu);
    gpu.ptr += (UINT64)d->srvHeapCursor * d->srvDescriptorSize;
    d->srvHeapCursor++;

    ID3D12GraphicsCommandList* list = d->graphicsCommandList.list;
    if (d->boundPipeline->isCompute) {
        list->lpVtbl->SetComputeRootDescriptorTable(list, slot->rootParameterIndex, gpu);
    } else {
        list->lpVtbl->SetGraphicsRootDescriptorTable(list, slot->rootParameterIndex, gpu);
    }
    return true;
}

static bool DX12_Device_BindPushConstants(RHI_Device_Impl* self, uint32_t offset, uint32_t size, const void* data) {
    DX12Device* d = (DX12Device*)self;
    if (!d->boundPipeline || !data || !size || (offset & 3) || (size & 3)) return false;
    DX12PipelineLayout* layout = d->boundPipeline->layout;
    if (!layout->pushConstantSize || offset > layout->pushConstantSize || size > layout->pushConstantSize - offset) return false;
    ID3D12GraphicsCommandList* list = d->graphicsCommandList.list;
    UINT count = size / 4;
    if (d->boundPipeline->isCompute) list->lpVtbl->SetComputeRoot32BitConstants(list, layout->pushConstantParameter, count, data, offset / 4);
    else list->lpVtbl->SetGraphicsRoot32BitConstants(list, layout->pushConstantParameter, count, data, offset / 4);
    return true;
}

static bool DX12_Device_BindSampler(RHI_Device_Impl* self, uint32_t binding, RHI_Sampler_Impl* samplerImpl) {
    DX12Device* d = (DX12Device*)self;
    if (!d->boundPipeline || !samplerImpl) return false;

    const DX12BindingSlot* slot = DX12_FindBindingSlot(d->boundPipeline->layout, binding, RHI_BINDING_SAMPLER);
    if (!slot) return false;

    DX12Sampler* sampler = (DX12Sampler*)samplerImpl;
    if (!sampler->heap || d->samplerHeapCursor >= d->samplerHeapCapacity) return false;

    D3D12_CPU_DESCRIPTOR_HANDLE src;
    sampler->heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(sampler->heap, &src);

    D3D12_CPU_DESCRIPTOR_HANDLE dst;
    d->samplerHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(d->samplerHeap, &dst);
    dst.ptr += (SIZE_T)d->samplerHeapCursor * d->samplerDescriptorSize;

    d->device->lpVtbl->CopyDescriptorsSimple(d->device, 1, dst, src, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    D3D12_GPU_DESCRIPTOR_HANDLE gpu;
    d->samplerHeap->lpVtbl->GetGPUDescriptorHandleForHeapStart(d->samplerHeap, &gpu);
    gpu.ptr += (UINT64)d->samplerHeapCursor * d->samplerDescriptorSize;
    d->samplerHeapCursor++;

    ID3D12GraphicsCommandList* list = d->graphicsCommandList.list;
    if (d->boundPipeline->isCompute) {
        list->lpVtbl->SetComputeRootDescriptorTable(list, slot->rootParameterIndex, gpu);
    } else {
        list->lpVtbl->SetGraphicsRootDescriptorTable(list, slot->rootParameterIndex, gpu);
    }
    return true;
}

static void DX12_Transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, D3D12_RESOURCE_STATES* state, D3D12_RESOURCE_STATES target) {
    if (*state == target) return;
    if (*state == D3D12_RESOURCE_STATE_GENERIC_READ &&
        (target == D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER ||
         target == (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))) return;
    D3D12_RESOURCE_BARRIER barrier = {0};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = *state;
    barrier.Transition.StateAfter = target;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->lpVtbl->ResourceBarrier(list, 1, &barrier);
    *state = target;
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
    .bindBuffer = DX12_Device_BindBuffer,
    .bindImage = DX12_Device_BindImage,
    .bindSampler = DX12_Device_BindSampler,
    .bindPushConstants = DX12_Device_BindPushConstants,
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
        fprintf(stderr, "CreateDXGIFactory2 failed: hr=0x%08lX\n", (unsigned long)hr);
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
            continue;
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
        fprintf(stderr, "D3D12CreateDevice failed: hr=0x%08lX\n", (unsigned long)hr);
        dxDevice->adapter->lpVtbl->Release(dxDevice->adapter);
        free(dxDevice);
        return false;
    }

    bool ok = true;
    HRESULT stepHr;

    ok &= DX12_CreateQueue(dxDevice->device, D3D12_COMMAND_LIST_TYPE_DIRECT,  &dxDevice->graphicsQueue);
    ok &= DX12_CreateQueue(dxDevice->device, D3D12_COMMAND_LIST_TYPE_COMPUTE, &dxDevice->computeQueue);
    ok &= DX12_CreateQueue(dxDevice->device, D3D12_COMMAND_LIST_TYPE_COPY,    &dxDevice->transferQueue);
    ok &= DX12_CommandList_Init(dxDevice->device, D3D12_COMMAND_LIST_TYPE_DIRECT,  &dxDevice->graphicsCommandList);
    ok &= DX12_CommandList_Init(dxDevice->device, D3D12_COMMAND_LIST_TYPE_COMPUTE, &dxDevice->computeCommandList);
    ok &= DX12_CommandList_Init(dxDevice->device, D3D12_COMMAND_LIST_TYPE_COPY,    &dxDevice->transferCommandList);

    if (!ok) {
        fprintf(stderr, "DX12_Device_Create: queue/command-list setup failed\n");
        DX12_DumpDebugMessages(dxDevice->device);
    }

    if (ok) {
        stepHr = dxDevice->device->lpVtbl->CreateFence(
            dxDevice->device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&dxDevice->transferFence);
        DX12_CHECK(stepHr, dxDevice->device, "CreateFence (transfer)");
        ok &= SUCCEEDED(stepHr);
    }
    if (ok) {
        dxDevice->transferFenceEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
        ok &= dxDevice->transferFenceEvent != NULL;
        if (!dxDevice->transferFenceEvent) {
            fprintf(stderr, "CreateEventEx (transfer fence) failed\n");
        }
    }
    dxDevice->nextTransferFenceValue = 1;
    if (ok) {
        stepHr = dxDevice->device->lpVtbl->CreateFence(dxDevice->device, 0, D3D12_FENCE_FLAG_NONE,
            &IID_ID3D12Fence, (void**)&dxDevice->frameFence);
        ok &= SUCCEEDED(stepHr);
        dxDevice->frameFenceEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
        ok &= dxDevice->frameFenceEvent != NULL;
        dxDevice->nextFrameFenceValue = 1;
    }

    if (ok) {
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {0};
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.NumDescriptors = DX12_SRV_HEAP_CAPACITY;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        stepHr = dxDevice->device->lpVtbl->CreateDescriptorHeap(
            dxDevice->device, &srvHeapDesc, &IID_ID3D12DescriptorHeap, (void**)&dxDevice->srvHeap);
        DX12_CHECK(stepHr, dxDevice->device, "CreateDescriptorHeap (SRV)");
        ok &= SUCCEEDED(stepHr);
        if (ok) {
            dxDevice->srvDescriptorSize = dxDevice->device->lpVtbl->GetDescriptorHandleIncrementSize(
                dxDevice->device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            dxDevice->srvHeapCapacity = DX12_SRV_HEAP_CAPACITY;
        }
    }

    if (ok) {
        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {0};
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.NumDescriptors = DX12_SAMPLER_HEAP_CAPACITY;
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        stepHr = dxDevice->device->lpVtbl->CreateDescriptorHeap(
            dxDevice->device, &samplerHeapDesc, &IID_ID3D12DescriptorHeap, (void**)&dxDevice->samplerHeap);
        DX12_CHECK(stepHr, dxDevice->device, "CreateDescriptorHeap (Sampler)");
        ok &= SUCCEEDED(stepHr);
        if (ok) {
            dxDevice->samplerDescriptorSize = dxDevice->device->lpVtbl->GetDescriptorHandleIncrementSize(
                dxDevice->device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            dxDevice->samplerHeapCapacity = DX12_SAMPLER_HEAP_CAPACITY;
        }
    }

    if (!ok) {
        DX12_Device_Destroy(&dxDevice->base);
        return false;
    }

    *device = &dxDevice->base;
    return true;
}
