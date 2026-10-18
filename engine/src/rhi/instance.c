#include "rhi/instance.h"
#include "rhi/instance_internal.h" 
#include "core/handle_internal.h"
#include <stdbool.h>

#ifdef RHI_BACKEND_DX12_AVAILABLE
#include "dx12/dx12.h"
#endif
#ifdef RHI_BACKEND_VULKAN_AVAILABLE
#include "vulkan/vulkan_backend.h"
#endif

DEFINE_HANDLE_POOL(RHI_Instance, RHI_Instance_Impl*);
static RHI_InstancePool s_instances;

RHI_Instance RHI_Instance_Create(RHI_Backend backend, bool debug) {
    RHI_Instance handle = RHI_InstancePool_Alloc(&s_instances);
    RHI_Instance_Impl** impl = RHI_InstancePool_Resolve(&s_instances, handle);

    RHI_Backend selected_backend = backend;
    if (selected_backend == RHI_BACKEND_AUTO) {
#ifdef RHI_BACKEND_DX12_AVAILABLE
        selected_backend = RHI_BACKEND_DX12;
#elif defined(RHI_BACKEND_VULKAN_AVAILABLE)
            selected_backend = RHI_BACKEND_VULKAN;
#else
        selected_backend = RHI_BACKEND_AUTO;
#endif
    }

    bool result = false;
    switch (selected_backend) {
        case RHI_BACKEND_DX12:
#ifdef RHI_BACKEND_DX12_AVAILABLE
            result = DX12_Instance_Create(debug, impl);
#else
            result = false;
#endif
            break;
        case RHI_BACKEND_VULKAN:
#ifdef RHI_BACKEND_VULKAN_AVAILABLE
            result = Vulkan_Instance_Create(debug, impl);
#endif
            break;
        default:
            result = false;
            break;
    }

    if (!result) {
        RHI_InstancePool_Free(&s_instances, handle);
        return RHI_Instance_Invalid();
    }

    (*impl)->backend = selected_backend;
    return handle;
}

void RHI_Instance_Destroy(RHI_Instance handle) {
    RHI_Instance_Impl** impl_ptr = RHI_InstancePool_Resolve(&s_instances, handle);
    if (!impl_ptr || !*impl_ptr) return;

    (*impl_ptr)->vtable->destroy(*impl_ptr);
    *impl_ptr = NULL;
    RHI_InstancePool_Free(&s_instances, handle);
}

RHI_Backend RHI_Instance_Get_Backend(RHI_Instance handle) {
    RHI_Instance_Impl** impl_ptr = RHI_InstancePool_Resolve(&s_instances, handle);
    if (!impl_ptr || !*impl_ptr) return RHI_BACKEND_AUTO;
    return (*impl_ptr)->backend;
}

// internal
RHI_Instance_Impl** RHI_Instance_Get_Impl(RHI_Instance handle) {
    return RHI_InstancePool_Resolve(&s_instances, handle);
}
