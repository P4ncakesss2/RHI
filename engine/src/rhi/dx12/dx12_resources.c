#include "dx12.h"
#include "rhi/sampler_internal.h"

#include <stdlib.h>
#include <string.h>

static void DX12_Buffer_Destroy(RHI_Buffer_Impl *self) {
    DX12Buffer *buffer = (DX12Buffer *)self;
    if (buffer->resource) {
        buffer->resource->lpVtbl->Release(buffer->resource);
    }
    free(buffer);
}

static const RHI_Buffer_Vtable dx12BufferVtable = {
    .destroy = DX12_Buffer_Destroy,
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

    D3D12_HEAP_PROPERTIES heapProperties = {0};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
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

    HRESULT hr = dxDevice->device->lpVtbl->CreateCommittedResource(
        dxDevice->device, &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
        (void **)&buffer->resource
    );
    if (FAILED(hr)) {
        free(buffer);
        return false;
    }

    if (desc->initialData) {
        void *mapped = NULL;
        D3D12_RANGE readRange = {0, 0};
        hr = buffer->resource->lpVtbl->Map(buffer->resource, 0, &readRange, &mapped);
        if (FAILED(hr)) {
            buffer->resource->lpVtbl->Release(buffer->resource);
            free(buffer);
            return false;
        }
        memcpy(mapped, desc->initialData, desc->size);
        buffer->resource->lpVtbl->Unmap(buffer->resource, 0, NULL);
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

static const RHI_Image_Vtable dx12ImageVtable = {
    .destroy = DX12_Image_Destroy,
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
    
    if (viewFormat == DXGI_FORMAT_UNKNOWN || (desc->renderTarget && (isDepthFormat || isBlockCompressed))) {
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
        (desc->sampled && !(formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE))) {
        return false;
    }

    DX12Image *image = calloc(1, sizeof(*image));
    if (!image) {
        return false;
    }
    
    image->base.vtable = &dx12ImageVtable;
    image->base.info = (RHI_ImageInfo){desc->width, desc->height, desc->format};
    image->state = D3D12_RESOURCE_STATE_COMMON;

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
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    
    if (isDepthFormat) {
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    } else if (desc->renderTarget) {
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }

    D3D12_CLEAR_VALUE clearValue = {0};
    clearValue.Format = viewFormat;
    clearValue.DepthStencil.Depth = 1.0f;

    hr = dxDevice->device->lpVtbl->CreateCommittedResource(
        dxDevice->device, &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON, isDepthFormat ? &clearValue : NULL,
        &IID_ID3D12Resource, (void **)&image->resource
    );
    if (FAILED(hr)) {
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

    if (desc->sampled) {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {0};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        
        hr = dxDevice->device->lpVtbl->CreateDescriptorHeap(
            dxDevice->device, &heapDesc, &IID_ID3D12DescriptorHeap, (void **)&image->srvHeap
        );
        if (FAILED(hr)) {
            DX12_Image_Destroy(&image->base);
            return false;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {0};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DX12_GetShaderResourceFormat(desc->format);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle;
        image->srvHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(image->srvHeap, &srvHandle);
        dxDevice->device->lpVtbl->CreateShaderResourceView(dxDevice->device, image->resource, &srvDesc, srvHandle);
    }

    *out = &image->base;
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