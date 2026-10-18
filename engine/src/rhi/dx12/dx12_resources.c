#include "dx12.h"
#include "rhi/sampler_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void DX12_Buffer_Destroy(RHI_Buffer_Impl *self) {
    DX12Buffer *buffer = (DX12Buffer *)self;
    if (buffer->resource) {
        buffer->resource->lpVtbl->Release(buffer->resource);
    }
    free(buffer);
}

static bool DX12_CopyBufferRegionAndWait(DX12Device* dxDevice,
                                   ID3D12Resource* dstResource, size_t dstOffset,
                                   ID3D12Resource* srcResource, size_t srcOffset,
                                   size_t size, D3D12_RESOURCE_STATES* dstState,
                                   D3D12_RESOURCE_STATES* srcState) {
    if (!dxDevice || !dstResource || !srcResource || size == 0) {
        return false;
    }
    if (*dstState != D3D12_RESOURCE_STATE_COMMON ||
        (*srcState != D3D12_RESOURCE_STATE_GENERIC_READ && *srcState != D3D12_RESOURCE_STATE_COMMON)) {
        return false;
    }

    if (!DX12_CommandList_Begin(&dxDevice->transferCommandList, 0)) {
        fprintf(stderr, "DX12_CopyBufferRegionAndWait: failed to begin transfer command list\n");
        return false;
    }
    ID3D12GraphicsCommandList* list = dxDevice->transferCommandList.list;
    D3D12_RESOURCE_BARRIER barrier = {0};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = dstResource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->lpVtbl->ResourceBarrier(list, 1, &barrier);
    if (*srcState == D3D12_RESOURCE_STATE_COMMON) {
        barrier.Transition.pResource = srcResource;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        list->lpVtbl->ResourceBarrier(list, 1, &barrier);
    }
    list->lpVtbl->CopyBufferRegion(list, dstResource, dstOffset, srcResource, srcOffset, size);
    barrier.Transition.pResource = dstResource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    list->lpVtbl->ResourceBarrier(list, 1, &barrier);
    if (*srcState == D3D12_RESOURCE_STATE_COMMON) {
        barrier.Transition.pResource = srcResource;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        list->lpVtbl->ResourceBarrier(list, 1, &barrier);
    }

    DX12_CommandList_End(&dxDevice->transferCommandList, dxDevice->transferQueue);

    UINT64 fenceValue = dxDevice->nextTransferFenceValue++;
    HRESULT hr = dxDevice->transferQueue->lpVtbl->Signal(dxDevice->transferQueue, dxDevice->transferFence, fenceValue);
    if (FAILED(hr)) {
        fprintf(stderr, "DX12_CopyBufferRegionAndWait: Signal failed: hr=0x%08lX\n", (unsigned long)hr);
        DX12_DumpDebugMessages(dxDevice->device);
        return false;
    }

    if (dxDevice->transferFence->lpVtbl->GetCompletedValue(dxDevice->transferFence) < fenceValue) {
        dxDevice->transferFence->lpVtbl->SetEventOnCompletion(dxDevice->transferFence, fenceValue, dxDevice->transferFenceEvent);
        WaitForSingleObject(dxDevice->transferFenceEvent, INFINITE);
    }

    return true;
}

static bool DX12_Buffer_Upload(RHI_Device device, RHI_Buffer_Impl *impl, size_t offset, size_t size, const void *data) {
    (void)device; 
    if (!impl || !data || !size) {
        return false;
    }

    DX12Buffer *buffer = (DX12Buffer *)impl;
    if (offset > buffer->base.size || size > buffer->base.size - offset) {
        fprintf(stderr, "DX12_Buffer_Upload: offset/size out of bounds\n");
        return false;
    }

    if (!(buffer->base.usage & RHI_BUFFER_TRANSFER_SRC)) {
        fprintf(stderr, "DX12_Buffer_Upload: buffer missing RHI_BUFFER_TRANSFER_SRC usage\n");
        return false;
    }

    void *mapped = NULL;
    D3D12_RANGE readRange = {0, 0};
    HRESULT hr = buffer->resource->lpVtbl->Map(buffer->resource, 0, &readRange, &mapped);
    if (FAILED(hr)) {
        fprintf(stderr, "DX12_Buffer_Upload: Map failed: hr=0x%08lX\n", (unsigned long)hr);
        return false;
    }
    memcpy((uint8_t *)mapped + offset, data, size);
    buffer->resource->lpVtbl->Unmap(buffer->resource, 0, NULL);
    return true;
}

static bool DX12_Buffer_Copy(RHI_Device device,
                       RHI_Buffer_Impl *dstImpl, size_t dstOffset,
                       RHI_Buffer_Impl *srcImpl, size_t srcOffset,
                       size_t size) {
    if (!dstImpl || !srcImpl || !size) {
        return false;
    }

    DX12Buffer *dst = (DX12Buffer *)dstImpl;
    DX12Buffer *src = (DX12Buffer *)srcImpl;

    if (dstOffset > dst->base.size || size > dst->base.size - dstOffset ||
        srcOffset > src->base.size || size > src->base.size - srcOffset) {
        fprintf(stderr, "DX12_Buffer_Copy: offset/size out of bounds\n");
        return false;
    }

    RHI_Device_Impl **deviceImpl = RHI_Device_Get_Impl(device);
    if (!deviceImpl || !*deviceImpl) {
        return false;
    }
    DX12Device *dxDevice = (DX12Device *)*deviceImpl;

    bool copied = DX12_CopyBufferRegionAndWait(dxDevice, dst->resource, dstOffset, src->resource, srcOffset, size,
                                               &dst->state, &src->state);
    if (copied) dst->state = D3D12_RESOURCE_STATE_COMMON;
    return copied;
}

static const RHI_Buffer_Vtable dx12BufferVtable = {
    .destroy = DX12_Buffer_Destroy,
    .upload = DX12_Buffer_Upload,
    .copy = DX12_Buffer_Copy,
};

bool DX12_Buffer_Create(RHI_Device device, const RHI_BufferDesc *desc, RHI_Buffer_Impl **out) {
    RHI_Device_Impl **deviceImpl = RHI_Device_Get_Impl(device);
    if (!deviceImpl || !*deviceImpl) {
        return false;
    }

    DX12Device *dxDevice = (DX12Device *)*deviceImpl;
    DX12Buffer *buffer = calloc(1, sizeof(*buffer));
    if (!buffer) {
        return false;
    }

    buffer->base.vtable = &dx12BufferVtable;
    buffer->base.size = desc->size;
    buffer->base.usage = desc->usage;
    buffer->state = (desc->usage & RHI_BUFFER_TRANSFER_SRC) ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;

    const bool isCpuStaging = (desc->usage & RHI_BUFFER_TRANSFER_SRC) != 0;

    D3D12_HEAP_PROPERTIES heapProperties = {0};
    heapProperties.Type = isCpuStaging ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CreationNodeMask = 1;
    heapProperties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resourceDesc = {0};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = desc->size;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (desc->usage & RHI_BUFFER_STORAGE) resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    const D3D12_RESOURCE_STATES initialState = isCpuStaging ? D3D12_RESOURCE_STATE_GENERIC_READ
                                                              : D3D12_RESOURCE_STATE_COMMON;

    HRESULT hr = dxDevice->device->lpVtbl->CreateCommittedResource(
        dxDevice->device, &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        initialState, NULL, &IID_ID3D12Resource,
        (void **)&buffer->resource
    );
    if (FAILED(hr)) {
        fprintf(stderr, "CreateCommittedResource (buffer, size=%zu, usage=0x%X) failed: hr=0x%08lX\n",
                desc->size, desc->usage, (unsigned long)hr);
        DX12_DumpDebugMessages(dxDevice->device);
        free(buffer);
        return false;
    }

    buffer->vertexView.BufferLocation = buffer->resource->lpVtbl->GetGPUVirtualAddress(buffer->resource);
    buffer->vertexView.SizeInBytes = (UINT)desc->size;
    *out = &buffer->base;
    return true;
}

DXGI_FORMAT DX12_ImageFormat_To_DXGI(RHI_ImageFormat format) {
    switch (format) {
        case RHI_IMAGE_FORMAT_R8_UNORM:
            return DXGI_FORMAT_R8_UNORM;
        case RHI_IMAGE_FORMAT_R8_SNORM:
            return DXGI_FORMAT_R8_SNORM;
        case RHI_IMAGE_FORMAT_R8_UINT:
            return DXGI_FORMAT_R8_UINT;
        case RHI_IMAGE_FORMAT_R8_SINT:
            return DXGI_FORMAT_R8_SINT;
        case RHI_IMAGE_FORMAT_RG8_UNORM:
            return DXGI_FORMAT_R8G8_UNORM;
        case RHI_IMAGE_FORMAT_RG8_SNORM:
            return DXGI_FORMAT_R8G8_SNORM;
        case RHI_IMAGE_FORMAT_RG8_UINT:
            return DXGI_FORMAT_R8G8_UINT;
        case RHI_IMAGE_FORMAT_RG8_SINT:
            return DXGI_FORMAT_R8G8_SINT;
        case RHI_IMAGE_FORMAT_RGBA8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case RHI_IMAGE_FORMAT_RGBA8_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case RHI_IMAGE_FORMAT_RGBA8_SNORM:
            return DXGI_FORMAT_R8G8B8A8_SNORM;
        case RHI_IMAGE_FORMAT_RGBA8_UINT:
            return DXGI_FORMAT_R8G8B8A8_UINT;
        case RHI_IMAGE_FORMAT_RGBA8_SINT:
            return DXGI_FORMAT_R8G8B8A8_SINT;
        case RHI_IMAGE_FORMAT_BGRA8_UNORM:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case RHI_IMAGE_FORMAT_BGRA8_SRGB:
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case RHI_IMAGE_FORMAT_R16_UNORM:
            return DXGI_FORMAT_R16_UNORM;
        case RHI_IMAGE_FORMAT_R16_SNORM:
            return DXGI_FORMAT_R16_SNORM;
        case RHI_IMAGE_FORMAT_R16_UINT:
            return DXGI_FORMAT_R16_UINT;
        case RHI_IMAGE_FORMAT_R16_SINT:
            return DXGI_FORMAT_R16_SINT;
        case RHI_IMAGE_FORMAT_R16_FLOAT:
            return DXGI_FORMAT_R16_FLOAT;
        case RHI_IMAGE_FORMAT_RG16_UNORM:
            return DXGI_FORMAT_R16G16_UNORM;
        case RHI_IMAGE_FORMAT_RG16_SNORM:
            return DXGI_FORMAT_R16G16_SNORM;
        case RHI_IMAGE_FORMAT_RG16_UINT:
            return DXGI_FORMAT_R16G16_UINT;
        case RHI_IMAGE_FORMAT_RG16_SINT:
            return DXGI_FORMAT_R16G16_SINT;
        case RHI_IMAGE_FORMAT_RG16_FLOAT:
            return DXGI_FORMAT_R16G16_FLOAT;
        case RHI_IMAGE_FORMAT_RGBA16_UNORM:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        case RHI_IMAGE_FORMAT_RGBA16_SNORM:
            return DXGI_FORMAT_R16G16B16A16_SNORM;
        case RHI_IMAGE_FORMAT_RGBA16_UINT:
            return DXGI_FORMAT_R16G16B16A16_UINT;
        case RHI_IMAGE_FORMAT_RGBA16_SINT:
            return DXGI_FORMAT_R16G16B16A16_SINT;
        case RHI_IMAGE_FORMAT_RGBA16_FLOAT:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case RHI_IMAGE_FORMAT_R32_UINT:
            return DXGI_FORMAT_R32_UINT;
        case RHI_IMAGE_FORMAT_R32_SINT:
            return DXGI_FORMAT_R32_SINT;
        case RHI_IMAGE_FORMAT_R32_FLOAT:
            return DXGI_FORMAT_R32_FLOAT;
        case RHI_IMAGE_FORMAT_RG32_UINT:
            return DXGI_FORMAT_R32G32_UINT;
        case RHI_IMAGE_FORMAT_RG32_SINT:
            return DXGI_FORMAT_R32G32_SINT;
        case RHI_IMAGE_FORMAT_RG32_FLOAT:
            return DXGI_FORMAT_R32G32_FLOAT;
        case RHI_IMAGE_FORMAT_RGB32_UINT:
            return DXGI_FORMAT_R32G32B32_UINT;
        case RHI_IMAGE_FORMAT_RGB32_SINT:
            return DXGI_FORMAT_R32G32B32_SINT;
        case RHI_IMAGE_FORMAT_RGB32_FLOAT:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case RHI_IMAGE_FORMAT_RGBA32_UINT:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        case RHI_IMAGE_FORMAT_RGBA32_SINT:
            return DXGI_FORMAT_R32G32B32A32_SINT;
        case RHI_IMAGE_FORMAT_RGBA32_FLOAT:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case RHI_IMAGE_FORMAT_RGB10A2_UNORM:
            return DXGI_FORMAT_R10G10B10A2_UNORM;
        case RHI_IMAGE_FORMAT_R11G11B10_FLOAT:
            return DXGI_FORMAT_R11G11B10_FLOAT;
        case RHI_IMAGE_FORMAT_D16_UNORM:
            return DXGI_FORMAT_D16_UNORM;
        case RHI_IMAGE_FORMAT_D24S8:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case RHI_IMAGE_FORMAT_D32_FLOAT:
            return DXGI_FORMAT_D32_FLOAT;
        case RHI_IMAGE_FORMAT_D32S8:
            return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        case RHI_IMAGE_FORMAT_BC1_UNORM:
            return DXGI_FORMAT_BC1_UNORM;
        case RHI_IMAGE_FORMAT_BC1_SRGB:
            return DXGI_FORMAT_BC1_UNORM_SRGB;
        case RHI_IMAGE_FORMAT_BC2_UNORM:
            return DXGI_FORMAT_BC2_UNORM;
        case RHI_IMAGE_FORMAT_BC2_SRGB:
            return DXGI_FORMAT_BC2_UNORM_SRGB;
        case RHI_IMAGE_FORMAT_BC3_UNORM:
            return DXGI_FORMAT_BC3_UNORM;
        case RHI_IMAGE_FORMAT_BC3_SRGB:
            return DXGI_FORMAT_BC3_UNORM_SRGB;
        case RHI_IMAGE_FORMAT_BC4_UNORM:
            return DXGI_FORMAT_BC4_UNORM;
        case RHI_IMAGE_FORMAT_BC4_SNORM:
            return DXGI_FORMAT_BC4_SNORM;
        case RHI_IMAGE_FORMAT_BC5_UNORM:
            return DXGI_FORMAT_BC5_UNORM;
        case RHI_IMAGE_FORMAT_BC5_SNORM:
            return DXGI_FORMAT_BC5_SNORM;
        case RHI_IMAGE_FORMAT_BC6H_UFLOAT:
            return DXGI_FORMAT_BC6H_UF16;
        case RHI_IMAGE_FORMAT_BC6H_SFLOAT:
            return DXGI_FORMAT_BC6H_SF16;
        case RHI_IMAGE_FORMAT_BC7_UNORM:
            return DXGI_FORMAT_BC7_UNORM;
        case RHI_IMAGE_FORMAT_BC7_SRGB:
            return DXGI_FORMAT_BC7_UNORM_SRGB;
        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}

static bool DX12_IsDepthFormat(RHI_ImageFormat format) {
    return format == RHI_IMAGE_FORMAT_D16_UNORM ||
           format == RHI_IMAGE_FORMAT_D24S8 ||
           format == RHI_IMAGE_FORMAT_D32_FLOAT ||
           format == RHI_IMAGE_FORMAT_D32S8;
}

static bool DX12_IsBlockCompressedFormat(RHI_ImageFormat format) {
    return format >= RHI_IMAGE_FORMAT_BC1_UNORM &&
           format <= RHI_IMAGE_FORMAT_BC7_SRGB;
}

static DXGI_FORMAT DX12_GetResourceFormat(RHI_ImageFormat format) {
    switch (format) {
        case RHI_IMAGE_FORMAT_D16_UNORM:
            return DXGI_FORMAT_R16_TYPELESS;
        case RHI_IMAGE_FORMAT_D24S8:
            return DXGI_FORMAT_R24G8_TYPELESS;
        case RHI_IMAGE_FORMAT_D32_FLOAT:
            return DXGI_FORMAT_R32_TYPELESS;
        case RHI_IMAGE_FORMAT_D32S8:
            return DXGI_FORMAT_R32G8X24_TYPELESS;
        default:
            return DX12_ImageFormat_To_DXGI(format);
    }
}

static DXGI_FORMAT DX12_GetShaderResourceFormat(RHI_ImageFormat format) {
    switch (format) {
        case RHI_IMAGE_FORMAT_D16_UNORM:
            return DXGI_FORMAT_R16_UNORM;
        case RHI_IMAGE_FORMAT_D24S8:
            return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case RHI_IMAGE_FORMAT_D32_FLOAT:
            return DXGI_FORMAT_R32_FLOAT;
        case RHI_IMAGE_FORMAT_D32S8:
            return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        default:
            return DX12_ImageFormat_To_DXGI(format);
    }
}

static void DX12_Image_Destroy(RHI_Image_Impl *self) {
    DX12Image *image = (DX12Image *)self;
    if (image->srvHeap) {
        image->srvHeap->lpVtbl->Release(image->srvHeap);
    }
    if (image->rtvHeap) {
        image->rtvHeap->lpVtbl->Release(image->rtvHeap);
    }
    if (image->resource) {
        image->resource->lpVtbl->Release(image->resource);
    }
    free(image);
}

static bool DX12_Image_Upload(RHI_Device device, RHI_Image_Impl* impl, uint32_t mipLevel, const void* data, size_t dataSize) {
    if (!impl || !data || mipLevel != 0) return false;
    RHI_Device_Impl** deviceImpl = RHI_Device_Get_Impl(device);
    if (!deviceImpl || !*deviceImpl) return false;
    DX12Device* dxDevice = (DX12Device*)*deviceImpl;
    DX12Image* image = (DX12Image*)impl;
    if (image->base.info.sampleCount > 1) return false;
    if (image->state != D3D12_RESOURCE_STATE_COMMON) return false;
    D3D12_RESOURCE_DESC textureDesc = {0};
    image->resource->lpVtbl->GetDesc(image->resource, &textureDesc);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT rows = 0;
    UINT64 rowSize = 0, uploadSize = 0;
    dxDevice->device->lpVtbl->GetCopyableFootprints(dxDevice->device, &textureDesc, 0, 1, 0, &footprint, &rows, &rowSize, &uploadSize);
    if (!rows || rowSize > SIZE_MAX || rowSize > SIZE_MAX / rows || dataSize < (size_t)rowSize * rows) return false;
    D3D12_HEAP_PROPERTIES heap = {0};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC bufferDesc = {0};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = uploadSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* staging = NULL;
    HRESULT hr = dxDevice->device->lpVtbl->CreateCommittedResource(dxDevice->device, &heap, D3D12_HEAP_FLAG_NONE,
        &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void**)&staging);
    if (FAILED(hr)) return false;
    uint8_t* mapped = NULL;
    D3D12_RANGE readRange = {0, 0};
    hr = staging->lpVtbl->Map(staging, 0, &readRange, (void**)&mapped);
    if (FAILED(hr)) { staging->lpVtbl->Release(staging); return false; }
    const uint8_t* src = (const uint8_t*)data;
    for (UINT row = 0; row < rows; ++row) memcpy(mapped + footprint.Offset + row * footprint.Footprint.RowPitch, src + row * rowSize, (size_t)rowSize);
    staging->lpVtbl->Unmap(staging, 0, NULL);
    if (!DX12_CommandList_Begin(&dxDevice->transferCommandList, 0)) { staging->lpVtbl->Release(staging); return false; }
    ID3D12GraphicsCommandList* list = dxDevice->transferCommandList.list;
    D3D12_RESOURCE_BARRIER barrier = {0};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = image->resource;
    barrier.Transition.StateBefore = image->state;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->lpVtbl->ResourceBarrier(list, 1, &barrier);
    D3D12_TEXTURE_COPY_LOCATION dst = {0}, srcLoc = {0};
    dst.pResource = image->resource;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.pResource = staging;
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint = footprint;
    list->lpVtbl->CopyTextureRegion(list, &dst, 0, 0, 0, &srcLoc, NULL);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    list->lpVtbl->ResourceBarrier(list, 1, &barrier);
    DX12_CommandList_End(&dxDevice->transferCommandList, dxDevice->transferQueue);
    UINT64 fence = dxDevice->nextTransferFenceValue++;
    hr = dxDevice->transferQueue->lpVtbl->Signal(dxDevice->transferQueue, dxDevice->transferFence, fence);
    if (SUCCEEDED(hr) && dxDevice->transferFence->lpVtbl->GetCompletedValue(dxDevice->transferFence) < fence) {
        dxDevice->transferFence->lpVtbl->SetEventOnCompletion(dxDevice->transferFence, fence, dxDevice->transferFenceEvent);
        WaitForSingleObject(dxDevice->transferFenceEvent, INFINITE);
    }
    image->state = D3D12_RESOURCE_STATE_COMMON;
    staging->lpVtbl->Release(staging);
    return SUCCEEDED(hr);
}

static const RHI_Image_Vtable dx12ImageVtable = {
    .destroy = DX12_Image_Destroy,
    .upload = DX12_Image_Upload,
    .resize = DX12_Image_Resize,
};

bool DX12_Image_Create(RHI_Device device, const RHI_ImageDesc *desc, RHI_Image_Impl **out) {
    RHI_Device_Impl **deviceImpl = RHI_Device_Get_Impl(device);
    if (!deviceImpl || !*deviceImpl) {
        return false;
    }

    DX12Device *dxDevice = (DX12Device *)*deviceImpl;
    DXGI_FORMAT viewFormat = DX12_ImageFormat_To_DXGI(desc->format);
    const bool isDepthFormat = DX12_IsDepthFormat(desc->format);
    const bool isBlockCompressed = DX12_IsBlockCompressedFormat(desc->format);
    const uint32_t sampleCount = desc->sampleCount ? desc->sampleCount : 1;
    
    if (viewFormat == DXGI_FORMAT_UNKNOWN ||
        ((desc->renderTarget || desc->storage) && (isDepthFormat || isBlockCompressed)) ||
        (sampleCount > 1 && ((sampleCount & (sampleCount - 1)) || desc->storage || !desc->renderTarget))) {
        fprintf(stderr, "DX12_Image_Create: unsupported format/renderTarget combination (format=%d)\n", (int)desc->format);
        return false;
    }

    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport = {0};
    formatSupport.Format = viewFormat;
    HRESULT hr = dxDevice->device->lpVtbl->CheckFeatureSupport(
        dxDevice->device, D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport)
    );
    if (FAILED(hr) ||
        !(formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) ||
        (desc->renderTarget && !(formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)) ||
        (isDepthFormat && !(formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)) ||
        (desc->storage && !(formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW)) ||
        (desc->sampled && !(formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE))) {
        fprintf(stderr, "DX12_Image_Create: CheckFeatureSupport failed or format unsupported for requested usage (format=%d, hr=0x%08lX)\n",
                (int)desc->format, (unsigned long)hr);
        if (FAILED(hr)) {
            DX12_DumpDebugMessages(dxDevice->device);
        }
        return false;
    }

    if (sampleCount > 1) {
        if (!(formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE)) return false;
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality = {0};
        quality.Format = viewFormat;
        quality.SampleCount = sampleCount;
        hr = dxDevice->device->lpVtbl->CheckFeatureSupport(dxDevice->device,
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &quality, sizeof(quality));
        if (FAILED(hr) || quality.NumQualityLevels == 0) return false;
    }

    DX12Image *image = calloc(1, sizeof(*image));
    if (!image) {
        return false;
    }
    
    image->base.vtable = &dx12ImageVtable;
    image->base.info = (RHI_ImageInfo){desc->width, desc->height, desc->format, sampleCount};
    image->state = D3D12_RESOURCE_STATE_COMMON;
    image->storage = desc->storage;
    image->renderTarget = desc->renderTarget;
    image->sampled = desc->sampled;

    D3D12_HEAP_PROPERTIES heapProperties = {0};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CreationNodeMask = 1;
    heapProperties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resourceDesc = {0};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = desc->width;
    resourceDesc.Height = desc->height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DX12_GetResourceFormat(desc->format);
    resourceDesc.SampleDesc.Count = sampleCount;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    
    if (isDepthFormat) {
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    } else if (desc->renderTarget) {
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (desc->storage) resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_CLEAR_VALUE clearValue = {0};
    clearValue.Format = viewFormat;
    clearValue.DepthStencil.Depth = 1.0f;

    hr = dxDevice->device->lpVtbl->CreateCommittedResource(
        dxDevice->device, &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON, isDepthFormat ? &clearValue : NULL,
        &IID_ID3D12Resource, (void **)&image->resource
    );
    if (FAILED(hr)) {
        fprintf(stderr, "CreateCommittedResource (image, %ux%u, format=%d) failed: hr=0x%08lX\n",
                desc->width, desc->height, (int)desc->format, (unsigned long)hr);
        DX12_DumpDebugMessages(dxDevice->device);
        free(image);
        return false;
    }

    if (desc->renderTarget || isDepthFormat) {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {0};
        heapDesc.Type = isDepthFormat ? D3D12_DESCRIPTOR_HEAP_TYPE_DSV : D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.NumDescriptors = 1;
        
        hr = dxDevice->device->lpVtbl->CreateDescriptorHeap(
            dxDevice->device, &heapDesc, &IID_ID3D12DescriptorHeap, (void **)&image->rtvHeap
        );
        if (FAILED(hr)) {
            fprintf(stderr, "CreateDescriptorHeap (image %s) failed: hr=0x%08lX\n",
                    isDepthFormat ? "DSV" : "RTV", (unsigned long)hr);
            DX12_DumpDebugMessages(dxDevice->device);
            DX12_Image_Destroy(&image->base);
            return false;
        }

        if (isDepthFormat) {
            image->rtvHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(image->rtvHeap, &image->dsv);
            dxDevice->device->lpVtbl->CreateDepthStencilView(dxDevice->device, image->resource, NULL, image->dsv);
        } else {
            image->rtvHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(image->rtvHeap, &image->rtv);
            dxDevice->device->lpVtbl->CreateRenderTargetView(dxDevice->device, image->resource, NULL, image->rtv);
        }
    }

    if (desc->sampled || desc->storage) {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {0};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = desc->sampled && desc->storage ? 2 : 1;
        
        hr = dxDevice->device->lpVtbl->CreateDescriptorHeap(
            dxDevice->device, &heapDesc, &IID_ID3D12DescriptorHeap, (void **)&image->srvHeap
        );
        if (FAILED(hr)) {
            fprintf(stderr, "CreateDescriptorHeap (image SRV) failed: hr=0x%08lX\n", (unsigned long)hr);
            DX12_DumpDebugMessages(dxDevice->device);
            DX12_Image_Destroy(&image->base);
            return false;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {0};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DX12_GetShaderResourceFormat(desc->format);
        if (sampleCount > 1) {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
        } else {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;
        image->srvHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(image->srvHeap, &srvHandle);
        if (desc->sampled) dxDevice->device->lpVtbl->CreateShaderResourceView(dxDevice->device, image->resource, &srvDesc, srvHandle);
        if (desc->storage) {
            image->uav = srvHandle;
            if (desc->sampled) image->uav.ptr += dxDevice->device->lpVtbl->GetDescriptorHandleIncrementSize(dxDevice->device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {0};
            uavDesc.Format = viewFormat;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            dxDevice->device->lpVtbl->CreateUnorderedAccessView(dxDevice->device, image->resource, NULL, &uavDesc, image->uav);
        }
    }

    *out = &image->base;
    return true;
}

bool DX12_Image_Resize(RHI_Device device, RHI_Image_Impl* impl, uint32_t width, uint32_t height) {
    if (!impl || !width || !height) return false;
    DX12Image* image = (DX12Image*)impl;
    if (image->owner) return false;
    if (image->base.info.width == width && image->base.info.height == height) return true;
    RHI_Device_Impl** deviceImpl = RHI_Device_Get_Impl(device);
    if (!deviceImpl || !*deviceImpl) return false;
    DX12Device* dxDevice = (DX12Device*)*deviceImpl;
    if (dxDevice->frameRecording || dxDevice->hasActiveRenderPass) return false;

    UINT64 fenceValue = dxDevice->nextFrameFenceValue++;
    HRESULT hr = dxDevice->graphicsQueue->lpVtbl->Signal(dxDevice->graphicsQueue, dxDevice->frameFence, fenceValue);
    if (FAILED(hr)) return false;
    if (dxDevice->frameFence->lpVtbl->GetCompletedValue(dxDevice->frameFence) < fenceValue) {
        hr = dxDevice->frameFence->lpVtbl->SetEventOnCompletion(dxDevice->frameFence, fenceValue, dxDevice->frameFenceEvent);
        if (FAILED(hr)) return false;
        WaitForSingleObject(dxDevice->frameFenceEvent, INFINITE);
    }

    RHI_ImageDesc desc = {
        .width = width,
        .height = height,
        .format = image->base.info.format,
        .renderTarget = image->renderTarget,
        .sampled = image->sampled,
        .storage = image->storage,
        .sampleCount = image->base.info.sampleCount,
    };
    RHI_Image_Impl* replacementImpl = NULL;
    if (!DX12_Image_Create(device, &desc, &replacementImpl)) return false;

    ID3D12Resource* oldResource = image->resource;
    ID3D12DescriptorHeap* oldRtvHeap = image->rtvHeap;
    ID3D12DescriptorHeap* oldSrvHeap = image->srvHeap;
    for (uint32_t i = 0; i < RHI_PIPELINE_MAX_BINDINGS; ++i) {
        if (dxDevice->boundUavResources[i] == oldResource) {
            dxDevice->boundUavResources[i] = NULL;
            dxDevice->pendingUavWrites[i] = false;
        }
    }
    *image = *(DX12Image*)replacementImpl;
    free(replacementImpl);
    if (oldSrvHeap) oldSrvHeap->lpVtbl->Release(oldSrvHeap);
    if (oldRtvHeap) oldRtvHeap->lpVtbl->Release(oldRtvHeap);
    if (oldResource) oldResource->lpVtbl->Release(oldResource);
    return true;
}

static void DX12_Sampler_Destroy(RHI_Sampler_Impl *self) {
    DX12Sampler *sampler = (DX12Sampler *)self;
    if (sampler->heap) {
        sampler->heap->lpVtbl->Release(sampler->heap);
    }
    free(sampler);
}

static const RHI_Sampler_Vtable dx12SamplerVtable = {
    .destroy = DX12_Sampler_Destroy,
};

static D3D12_TEXTURE_ADDRESS_MODE DX12_GetAddressMode(RHI_AddressMode mode) {
    switch (mode) {
        case RHI_ADDRESS_REPEAT:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case RHI_ADDRESS_MIRRORED_REPEAT:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case RHI_ADDRESS_CLAMP:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case RHI_ADDRESS_CLAMP_TO_BORDER:
            return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case RHI_ADDRESS_MIRROR_CLAMP:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        default:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

static D3D12_COMPARISON_FUNC DX12_GetSamplerCompare(RHI_SamplerCompareOp op) {
    static const D3D12_COMPARISON_FUNC functions[] = {
        D3D12_COMPARISON_FUNC_NEVER,
        D3D12_COMPARISON_FUNC_LESS,
        D3D12_COMPARISON_FUNC_EQUAL,
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_COMPARISON_FUNC_GREATER,
        D3D12_COMPARISON_FUNC_NOT_EQUAL,
        D3D12_COMPARISON_FUNC_GREATER_EQUAL,
        D3D12_COMPARISON_FUNC_ALWAYS,
    };
    return op <= RHI_SAMPLER_COMPARE_ALWAYS ? functions[op] : D3D12_COMPARISON_FUNC_ALWAYS;
}

static D3D12_FILTER DX12_GetFilter(const RHI_SamplerDesc *desc) {
    if (desc->anisotropyEnable) {
        return desc->compareEnable ? D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC;
    }

    const unsigned filterIndex = ((unsigned)desc->minFilter << 2) |
                                 ((unsigned)desc->magFilter << 1) |
                                 (unsigned)desc->mipFilter;
                                 
    static const D3D12_FILTER regularFilters[8] = {
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
        D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
        D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR,
        D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT,
        D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
        D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
    };
    
    static const D3D12_FILTER comparisonFilters[8] = {
        D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT,
        D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR,
        D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT,
        D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR,
        D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT,
        D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
        D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
        D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
    };
    
    return desc->compareEnable ? comparisonFilters[filterIndex] : regularFilters[filterIndex];
}

bool DX12_Sampler_Create(RHI_Device device, const RHI_SamplerDesc *desc, RHI_Sampler_Impl **out) {
    RHI_Device_Impl **deviceImpl = RHI_Device_Get_Impl(device);
    if (!deviceImpl || !*deviceImpl) {
        return false;
    }

    DX12Device *dxDevice = (DX12Device *)*deviceImpl;
    DX12Sampler *sampler = calloc(1, sizeof(*sampler));
    if (!sampler) {
        return false;
    }
    sampler->base.vtable = &dx12SamplerVtable;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {0};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    heapDesc.NumDescriptors = 1;
    
    HRESULT hr = dxDevice->device->lpVtbl->CreateDescriptorHeap(
        dxDevice->device, &heapDesc, &IID_ID3D12DescriptorHeap, (void **)&sampler->heap
    );
    if (FAILED(hr)) {
        fprintf(stderr, "CreateDescriptorHeap (sampler) failed: hr=0x%08lX\n", (unsigned long)hr);
        DX12_DumpDebugMessages(dxDevice->device);
        free(sampler);
        return false;
    }

    D3D12_SAMPLER_DESC samplerDesc = {0};
    samplerDesc.Filter = DX12_GetFilter(desc);
    samplerDesc.AddressU = DX12_GetAddressMode(desc->addressU);
    samplerDesc.AddressV = DX12_GetAddressMode(desc->addressV);
    samplerDesc.AddressW = DX12_GetAddressMode(desc->addressW);
    samplerDesc.MipLODBias = desc->lodBias;
    samplerDesc.MaxAnisotropy = desc->anisotropyEnable ? desc->maxAnisotropy : 1;
    samplerDesc.ComparisonFunc = desc->compareEnable ? DX12_GetSamplerCompare(desc->compareOp)
                                                     : D3D12_COMPARISON_FUNC_ALWAYS;
                                                     
    const bool isWhite = desc->borderColor == RHI_BORDER_OPAQUE_WHITE;
    const bool isOpaque = desc->borderColor != RHI_BORDER_TRANSPARENT_BLACK;
    samplerDesc.BorderColor[0] = isWhite ? 1.0f : 0.0f;
    samplerDesc.BorderColor[1] = samplerDesc.BorderColor[0];
    samplerDesc.BorderColor[2] = samplerDesc.BorderColor[0];
    samplerDesc.BorderColor[3] = isOpaque ? 1.0f : 0.0f;
    samplerDesc.MinLOD = desc->minLod;
    samplerDesc.MaxLOD = desc->maxLod;

    D3D12_CPU_DESCRIPTOR_HANDLE samplerHandle;
    sampler->heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(sampler->heap, &samplerHandle);
    dxDevice->device->lpVtbl->CreateSampler(dxDevice->device, &samplerDesc, samplerHandle);

    *out = &sampler->base;
    return true;
}
