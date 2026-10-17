#include "rhi/swapchain.h"
#include "rhi/instance.h"
#include "swapchain_internal.h"
#include "core/handle_internal.h"
#include "rhi/device.h"
#include <stdbool.h>

#ifdef RHI_BACKEND_DX12_AVAILABLE
#include "dx12/dx12.h"
#endif

DEFINE_HANDLE_POOL(RHI_Swapchain, RHI_Swapchain_Impl*);
static RHI_SwapchainPool s_swapchains;

RHI_Swapchain RHI_Swapchain_Create(RHI_Instance instance, RHI_Device device, Window* window) {
    RHI_Swapchain handle = RHI_SwapchainPool_Alloc(&s_swapchains);
    RHI_Swapchain_Impl** impl = RHI_SwapchainPool_Resolve(&s_swapchains, handle);

    bool result = false;
    RHI_Backend backend = RHI_Instance_Get_Backend(instance);

    switch (backend) {
        case RHI_BACKEND_DX12:
#ifdef RHI_BACKEND_DX12_AVAILABLE
            result = DX12_Swapchain_Create(device, window, impl);
#else
            result = false;
#endif
            break;
        default:
            result = false;
            break;
    }

    if (!result) {
        RHI_SwapchainPool_Free(&s_swapchains, handle);
        return RHI_Swapchain_Invalid();
    }

    return handle;
}

void RHI_Swapchain_Destroy(RHI_Swapchain handle) {
    RHI_Swapchain_Impl** impl_ptr = RHI_SwapchainPool_Resolve(&s_swapchains, handle);
    if (!impl_ptr || !*impl_ptr) return;

    (*impl_ptr)->vtable->destroy(*impl_ptr);
    *impl_ptr = NULL;
    RHI_SwapchainPool_Free(&s_swapchains, handle);
}

bool RHI_Swapchain_Resize(RHI_Swapchain handle, uint32_t width, uint32_t height) {
    RHI_Swapchain_Impl** impl_ptr = RHI_SwapchainPool_Resolve(&s_swapchains, handle);
    if (!impl_ptr || !*impl_ptr) return false;

    return (*impl_ptr)->vtable->resize(*impl_ptr, width, height);
}

uint32_t RHI_Swapchain_Acquire_Next_Image(RHI_Swapchain handle) {
    RHI_Swapchain_Impl** impl_ptr = RHI_SwapchainPool_Resolve(&s_swapchains, handle);
    if (!impl_ptr || !*impl_ptr) return UINT32_MAX;

    return (*impl_ptr)->vtable->acquireNextImage(*impl_ptr);
}

RHI_Image RHI_Swapchain_Get_Current_Image(RHI_Swapchain handle) {
    RHI_Swapchain_Impl** impl_ptr = RHI_SwapchainPool_Resolve(&s_swapchains, handle);
    if (!impl_ptr || !*impl_ptr) return RHI_Image_Invalid();
    return (*impl_ptr)->vtable->getCurrentImage(*impl_ptr);
}

void RHI_Swapchain_Present(RHI_Swapchain handle) {
    RHI_Swapchain_Impl** impl_ptr = RHI_SwapchainPool_Resolve(&s_swapchains, handle);
    if (!impl_ptr || !*impl_ptr) return;

    (*impl_ptr)->vtable->present(*impl_ptr);
}

// internal
RHI_Swapchain_Impl** RHI_Swapchain_Get_Impl(RHI_Swapchain handle) {
    return RHI_SwapchainPool_Resolve(&s_swapchains, handle);
}
