#include "rhi/device.h"
#include "rhi/device_internal.h" 
#include "rhi/pipeline_internal.h"
#include "core/handle_internal.h"
#include "rhi/instance.h"
#include <stdbool.h>

#ifdef RHI_BACKEND_DX12_AVAILABLE
#include "dx12/dx12.h"
#endif

DEFINE_HANDLE_POOL(RHI_Device, RHI_Device_Impl*);
static RHI_DevicePool s_devices;

RHI_Device RHI_Device_Create(RHI_Instance instance) {
    RHI_Device handle = RHI_DevicePool_Alloc(&s_devices);
    RHI_Device_Impl** impl = RHI_DevicePool_Resolve(&s_devices, handle);

    bool result = false;
    RHI_Backend backend = RHI_Instance_Get_Backend(instance);
    switch(backend) {
        case RHI_BACKEND_DX12:
#ifdef RHI_BACKEND_DX12_AVAILABLE
            result = DX12_Device_Create(instance, impl);
#else
            result = false;
#endif
            break;
        default:
            result = false;
            break;
    }

    if (!result) {
        RHI_DevicePool_Free(&s_devices, handle);
        return RHI_Device_Invalid();
    }

    return handle;
}

void RHI_Device_Destroy(RHI_Device handle) {
    RHI_Device_Impl** impl_ptr = RHI_DevicePool_Resolve(&s_devices, handle);
    if (!impl_ptr || !*impl_ptr) return;

    (*impl_ptr)->vtable->destroy(*impl_ptr);
    *impl_ptr = NULL;
    RHI_DevicePool_Free(&s_devices, handle);
}

bool RHI_Device_Begin_Frame(RHI_Device handle, uint32_t frameIndex) {
    RHI_Device_Impl** impl_ptr = RHI_DevicePool_Resolve(&s_devices, handle);
    if (!impl_ptr || !*impl_ptr) return false;

    return (*impl_ptr)->vtable->beginFrame(*impl_ptr, frameIndex);
}

void RHI_Device_End_Frame(RHI_Device handle) {
    RHI_Device_Impl** impl_ptr = RHI_DevicePool_Resolve(&s_devices, handle);
    if (!impl_ptr || !*impl_ptr) return;

    (*impl_ptr)->vtable->endFrame(*impl_ptr);
}

bool RHI_Device_Begin_Render_Pass(RHI_Device handle, const RHI_RenderPass* pass) {
    RHI_Device_Impl** impl = RHI_DevicePool_Resolve(&s_devices, handle);
    if (!impl || !*impl || !pass) return false;
    return (*impl)->vtable->beginRenderPass(*impl, pass);
}

void RHI_Device_End_Render_Pass(RHI_Device handle) {
    RHI_Device_Impl** impl = RHI_DevicePool_Resolve(&s_devices, handle);
    if (impl && *impl) (*impl)->vtable->endRenderPass(*impl);
}

void RHI_Device_Bind_Pipeline(RHI_Device device, RHI_Pipeline pipeline) {
    RHI_Device_Impl** device_impl = RHI_DevicePool_Resolve(&s_devices, device);
    if (!device_impl || !*device_impl) return;

    RHI_Pipeline_Impl** pipeline_impl = RHI_Pipeline_Get_Impl(pipeline);
    if (!pipeline_impl || !*pipeline_impl) return;

    (*device_impl)->vtable->bindPipeline(*device_impl, *pipeline_impl);
}

void RHI_Device_Draw(RHI_Device device, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    RHI_Device_Impl** device_impl = RHI_DevicePool_Resolve(&s_devices, device);
    if (!device_impl || !*device_impl) return;

    (*device_impl)->vtable->draw(*device_impl, vertexCount, instanceCount, firstVertex, firstInstance);
}

void RHI_Device_Dispatch(RHI_Device device, uint32_t x, uint32_t y, uint32_t z) {
    RHI_Device_Impl** impl_ptr = RHI_DevicePool_Resolve(&s_devices, device);
    if (!impl_ptr || !*impl_ptr) return;
    (*impl_ptr)->vtable->dispatch(*impl_ptr, x, y, z);
}

bool RHI_Device_Bind_Vertex_Buffer(RHI_Device device, uint32_t slot, RHI_Buffer buffer, uint32_t stride) {
    RHI_Device_Impl** d = RHI_DevicePool_Resolve(&s_devices, device);
    RHI_Buffer_Impl** b = RHI_Buffer_Get_Impl(buffer);
    return d && *d && b && *b && (*d)->vtable->bindVertexBuffer(*d, slot, *b, stride);
}

bool RHI_Device_Bind_Index_Buffer(RHI_Device device, RHI_Buffer buffer, bool index32Bit) {
    RHI_Device_Impl** d = RHI_DevicePool_Resolve(&s_devices, device);
    RHI_Buffer_Impl** b = RHI_Buffer_Get_Impl(buffer);
    return d && *d && b && *b && (*d)->vtable->bindIndexBuffer(*d, *b, index32Bit);
}

void RHI_Device_Draw_Indexed(RHI_Device device, uint32_t n, uint32_t instances, uint32_t first, int32_t offset, uint32_t firstInstance) {
    RHI_Device_Impl** d = RHI_DevicePool_Resolve(&s_devices, device);
    if (d && *d) {
        (*d)->vtable->drawIndexed(*d, n, instances, first, offset, firstInstance);
    }
}

// internal
RHI_Device_Impl** RHI_Device_Get_Impl(RHI_Device handle) {
    return  RHI_DevicePool_Resolve(&s_devices, handle);
}
