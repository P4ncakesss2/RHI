#include "rhi/pipeline.h"
#include "rhi/pipeline_internal.h"
#include "rhi/instance.h"
#include "core/handle_internal.h"
#include <stdbool.h>

#ifdef RHI_BACKEND_DX12_AVAILABLE
#include "dx12/dx12.h"
#endif
#ifdef RHI_BACKEND_VULKAN_AVAILABLE
#include "vulkan/vulkan_backend.h"
#endif

DEFINE_HANDLE_POOL(RHI_PipelineLayout, RHI_PipelineLayout_Impl*);
static RHI_PipelineLayoutPool s_pipelineLayouts;

DEFINE_HANDLE_POOL(RHI_Pipeline, RHI_Pipeline_Impl*);
static RHI_PipelinePool s_pipelines;

RHI_PipelineLayout RHI_PipelineLayout_Create(RHI_Instance instance, RHI_Device device, const RHI_PipelineLayoutDesc* desc) {
    RHI_PipelineLayout handle = RHI_PipelineLayoutPool_Alloc(&s_pipelineLayouts);
    RHI_PipelineLayout_Impl** impl = RHI_PipelineLayoutPool_Resolve(&s_pipelineLayouts, handle);

    bool result = false;
    RHI_Backend backend = RHI_Instance_Get_Backend(instance);
    switch (backend) {
        case RHI_BACKEND_DX12:
#ifdef RHI_BACKEND_DX12_AVAILABLE
            result = DX12_PipelineLayout_Create(device, desc, impl);
#else
            result = false;
#endif
            break;
        case RHI_BACKEND_VULKAN:
#ifdef RHI_BACKEND_VULKAN_AVAILABLE
            result = Vulkan_PipelineLayout_Create(device, desc, impl);
#endif
            break;
        default:
            result = false;
            break;
        case RHI_BACKEND_VULKAN:
#ifdef RHI_BACKEND_VULKAN_AVAILABLE
            result = Vulkan_Pipeline_Create(device, desc, impl);
#endif
            break;
    }

    if (!result) {
        RHI_PipelineLayoutPool_Free(&s_pipelineLayouts, handle);
        return RHI_PipelineLayout_Invalid();
    }

    return handle;
}

void RHI_PipelineLayout_Destroy(RHI_PipelineLayout handle) {
    RHI_PipelineLayout_Impl** impl_ptr = RHI_PipelineLayoutPool_Resolve(&s_pipelineLayouts, handle);
    if (!impl_ptr || !*impl_ptr) return;

    (*impl_ptr)->vtable->destroy(*impl_ptr);
    *impl_ptr = NULL;
    RHI_PipelineLayoutPool_Free(&s_pipelineLayouts, handle);
}

RHI_Pipeline RHI_Pipeline_Create(RHI_Instance instance, RHI_Device device, const RHI_PipelineDesc* desc) {
    RHI_Pipeline handle = RHI_PipelinePool_Alloc(&s_pipelines);
    RHI_Pipeline_Impl** impl = RHI_PipelinePool_Resolve(&s_pipelines, handle);

    bool result = false;
    RHI_Backend backend = RHI_Instance_Get_Backend(instance);
    switch (backend) {
        case RHI_BACKEND_DX12:
#ifdef RHI_BACKEND_DX12_AVAILABLE
            result = DX12_Pipeline_Create(device, desc, impl);
#else
            result = false;
#endif
            break;
        default:
            result = false;
            break;
    }

    if (!result) {
        RHI_PipelinePool_Free(&s_pipelines, handle);
        return RHI_Pipeline_Invalid();
    }

    return handle;
}

void RHI_Pipeline_Destroy(RHI_Pipeline handle) {
    RHI_Pipeline_Impl** impl_ptr = RHI_PipelinePool_Resolve(&s_pipelines, handle);
    if (!impl_ptr || !*impl_ptr) return;

    (*impl_ptr)->vtable->destroy(*impl_ptr);
    *impl_ptr = NULL;
    RHI_PipelinePool_Free(&s_pipelines, handle);
}

// internal
RHI_PipelineLayout_Impl** RHI_PipelineLayout_Get_Impl(RHI_PipelineLayout handle) {
    return RHI_PipelineLayoutPool_Resolve(&s_pipelineLayouts, handle);
}

RHI_Pipeline_Impl** RHI_Pipeline_Get_Impl(RHI_Pipeline handle) {
    return RHI_PipelinePool_Resolve(&s_pipelines, handle);
}
