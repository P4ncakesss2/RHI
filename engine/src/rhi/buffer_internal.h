#ifndef RHI_BUFFER_INTERNAL_H
#define RHI_BUFFER_INTERNAL_H

#include "rhi/handles.h"
typedef struct RHI_Buffer_Impl RHI_Buffer_Impl;
typedef struct { 
    void (*destroy)(RHI_Buffer_Impl*); 
} RHI_Buffer_Vtable;

struct RHI_Buffer_Impl { 
    const RHI_Buffer_Vtable* vtable;
    size_t size;
    uint32_t usage;
};

RHI_Buffer_Impl** RHI_Buffer_Get_Impl(RHI_Buffer buffer);

#endif
