#ifndef SWAPCHAIN_H_INTERNAL
#define SWAPCHAIN_H_INTERNAL

#include "rhi/handles.h"
#include <stdint.h>

typedef struct RHI_Swapchain_Impl RHI_Swapchain_Impl;
typedef struct {
    void     (*destroy)(RHI_Swapchain_Impl* self);
    bool     (*resize)(RHI_Swapchain_Impl* self, uint32_t width, uint32_t height);
    uint32_t (*acquireNextImage)(RHI_Swapchain_Impl* self);
    RHI_Image (*getCurrentImage)(RHI_Swapchain_Impl* self);
    void     (*present)(RHI_Swapchain_Impl* self);
} RHI_Swapchain_Vtable;

struct RHI_Swapchain_Impl {
    const RHI_Swapchain_Vtable* vtable;
};

RHI_Swapchain_Impl** RHI_Swapchain_Get_Impl(RHI_Swapchain handle);

#endif
