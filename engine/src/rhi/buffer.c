#include "rhi/buffer.h"
#include "buffer_internal.h"
#include "rhi/instance.h"
#include "core/handle_internal.h"

#ifdef RHI_BACKEND_DX12_AVAILABLE
#include "dx12/dx12.h"
#endif
#ifdef RHI_BACKEND_VULKAN_AVAILABLE
#include "vulkan/vulkan_backend.h"
#endif

DEFINE_HANDLE_POOL(RHI_Buffer, RHI_Buffer_Impl*);
static RHI_BufferPool s_buffers;

RHI_Buffer_Impl** RHI_Buffer_Get_Impl(RHI_Buffer h) {
    return RHI_BufferPool_Resolve(&s_buffers, h);
}

RHI_Buffer RHI_Buffer_Create(RHI_Instance instance, RHI_Device device, const RHI_BufferDesc* desc) {
    if (!desc || !desc->size || !desc->usage) {
        return RHI_Buffer_Invalid();
    }

    RHI_Buffer h = RHI_BufferPool_Alloc(&s_buffers);
    RHI_Buffer_Impl** p = RHI_BufferPool_Resolve(&s_buffers, h);
    bool ok = false;

    switch (RHI_Instance_Get_Backend(instance)) {
        case RHI_BACKEND_DX12:
#ifdef RHI_BACKEND_DX12_AVAILABLE
            ok = DX12_Buffer_Create(device, desc, p);
#endif
            break;
        case RHI_BACKEND_VULKAN:
#ifdef RHI_BACKEND_VULKAN_AVAILABLE
            ok = Vulkan_Buffer_Create(device, desc, p);
#endif
            break;
        default:
            break;
    }

    if (!ok) {
        RHI_BufferPool_Free(&s_buffers, h);
        return RHI_Buffer_Invalid();
    }

    return h;
}

bool RHI_Buffer_Upload(RHI_Instance instance, RHI_Device device, RHI_Buffer h,
                        size_t offset, size_t size, const void* data) {
    (void)instance;
    if (!data || !size) {
        return false;
    }

    RHI_Buffer_Impl** p = RHI_Buffer_Get_Impl(h);
    if (!p || !*p || !(*p)->vtable->upload) {
        return false;
    }

    return (*p)->vtable->upload(device, *p, offset, size, data);
}

bool RHI_Buffer_Copy(RHI_Instance instance, RHI_Device device,
                      RHI_Buffer dst, size_t dstOffset,
                      RHI_Buffer src, size_t srcOffset,
                      size_t size) {
    (void)instance;
    if (!size) {
        return false;
    }

    RHI_Buffer_Impl** dstImpl = RHI_Buffer_Get_Impl(dst);
    RHI_Buffer_Impl** srcImpl = RHI_Buffer_Get_Impl(src);
    if (!dstImpl || !*dstImpl || !srcImpl || !*srcImpl) {
        return false;
    }

    if (!(*dstImpl)->vtable->copy) {
        return false;
    }

    return (*dstImpl)->vtable->copy(device, *dstImpl, dstOffset, *srcImpl, srcOffset, size);
}

void RHI_Buffer_Destroy(RHI_Buffer h) {
    RHI_Buffer_Impl** p = RHI_Buffer_Get_Impl(h);
    if (!p || !*p) return;

    (*p)->vtable->destroy(*p);
    *p = NULL;
    RHI_BufferPool_Free(&s_buffers, h);
}

size_t RHI_Buffer_Get_Size(RHI_Buffer h) {
    RHI_Buffer_Impl** p = RHI_Buffer_Get_Impl(h);
    return p && *p ? (*p)->size : 0;
}
