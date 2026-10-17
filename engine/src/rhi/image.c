#include "rhi/image.h"
#include "image_internal.h"
#include "core/handle_internal.h"
#include "rhi/instance.h"

#ifdef RHI_BACKEND_DX12_AVAILABLE
#include "dx12/dx12.h"
#endif

DEFINE_HANDLE_POOL(RHI_Image, RHI_Image_Impl*);

static RHI_ImagePool s_images;

RHI_Image RHI_Image_Register(RHI_Image_Impl* impl) {
    RHI_Image imageHandle = RHI_ImagePool_Alloc(&s_images);
    RHI_Image_Impl** imageImpl = RHI_ImagePool_Resolve(&s_images, imageHandle);
    if (imageImpl) {
        *imageImpl = impl;
    }
    return imageHandle;
}

void RHI_Image_Unregister(RHI_Image imageHandle) {
    RHI_Image_Impl** imageImpl = RHI_ImagePool_Resolve(&s_images, imageHandle);
    if (!imageImpl || !*imageImpl) {
        return;
    }
    
    if ((*imageImpl)->vtable && (*imageImpl)->vtable->destroy) {
        (*imageImpl)->vtable->destroy(*imageImpl);
    }
    
    *imageImpl = NULL;
    RHI_ImagePool_Free(&s_images, imageHandle);
}

RHI_Image_Impl** RHI_Image_Get_Impl(RHI_Image imageHandle) {
    return RHI_ImagePool_Resolve(&s_images, imageHandle);
}

RHI_ImageInfo RHI_Image_Get_Info(RHI_Image imageHandle) {
    RHI_Image_Impl** imageImpl = RHI_Image_Get_Impl(imageHandle);
    return imageImpl && *imageImpl ? (*imageImpl)->info : (RHI_ImageInfo){0};
}

RHI_Image RHI_Image_Create(RHI_Instance instance, RHI_Device device, const RHI_ImageDesc* desc) {
    if (!desc || !desc->width || !desc->height) {
        return RHI_Image_Invalid();
    }
    
    RHI_Image imageHandle = RHI_ImagePool_Alloc(&s_images);
    RHI_Image_Impl** imageImpl = RHI_ImagePool_Resolve(&s_images, imageHandle);
    bool ok = false;
    
    switch (RHI_Instance_Get_Backend(instance)) {
        case RHI_BACKEND_DX12:
#ifdef RHI_BACKEND_DX12_AVAILABLE
            ok = DX12_Image_Create(device, desc, imageImpl);
#endif
            break;
        default:
            break;
    }
    
    if (!ok) {
        RHI_ImagePool_Free(&s_images, imageHandle);
        return RHI_Image_Invalid();
    }
    
    return imageHandle;
}

void RHI_Image_Destroy(RHI_Image imageHandle) {
    RHI_Image_Impl** imageImpl = RHI_Image_Get_Impl(imageHandle);
    if (imageImpl && *imageImpl && (*imageImpl)->vtable) {
        RHI_Image_Unregister(imageHandle);
    }
}