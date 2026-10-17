#ifndef DEVICE_H_INTERNAL
#define DEVICE_H_INTERNAL

#include "rhi/render_pass.h"
#include "rhi/pipeline_internal.h"
#include "buffer_internal.h"
#include <stdint.h>

typedef struct RHI_Device_Impl RHI_Device_Impl;
typedef struct {
    void (*destroy)(RHI_Device_Impl* self);
    bool (*beginFrame)(RHI_Device_Impl* self, uint32_t frameIndex);
    bool (*beginRenderPass)(RHI_Device_Impl* self, const RHI_RenderPass* pass);
    void (*endRenderPass)(RHI_Device_Impl* self);
    void (*bindPipeline)(RHI_Device_Impl* self, RHI_Pipeline_Impl* pipeline);
    void (*draw)(RHI_Device_Impl* self, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
    void (*dispatch)(RHI_Device_Impl* self, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    bool (*bindVertexBuffer)(RHI_Device_Impl* self, uint32_t slot, RHI_Buffer_Impl* buffer, uint32_t stride);
    bool (*bindIndexBuffer)(RHI_Device_Impl* self, RHI_Buffer_Impl* buffer, bool index32Bit);
    void (*drawIndexed)(RHI_Device_Impl* self, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
    void (*endFrame)(RHI_Device_Impl* self);
} RHI_Device_Vtable;

struct RHI_Device_Impl {
    const RHI_Device_Vtable* vtable;
};

RHI_Device_Impl** RHI_Device_Get_Impl(RHI_Device handle);

#endif
