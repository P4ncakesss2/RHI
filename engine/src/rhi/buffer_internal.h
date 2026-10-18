#ifndef RHI_BUFFER_INTERNAL_H
#define RHI_BUFFER_INTERNAL_H

#include "rhi/handles.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct RHI_Buffer_Impl RHI_Buffer_Impl;

typedef struct { 
    void (*destroy)(RHI_Buffer_Impl*); 
    bool (*upload)(RHI_Device device, RHI_Buffer_Impl* impl, size_t offset, size_t size, const void* data);
    bool (*copy)(RHI_Device device, RHI_Buffer_Impl* dstImpl, size_t dstOffset, RHI_Buffer_Impl* srcImpl, size_t srcOffset, size_t size);
} RHI_Buffer_Vtable;

struct RHI_Buffer_Impl { 
    const RHI_Buffer_Vtable* vtable;
    size_t size;
    uint32_t usage;
};

RHI_Buffer_Impl** RHI_Buffer_Get_Impl(RHI_Buffer buffer);

#endif