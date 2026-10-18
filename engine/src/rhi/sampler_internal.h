#ifndef RHI_SAMPLER_INTERNAL_H
#define RHI_SAMPLER_INTERNAL_H

#include "rhi/handles.h"

typedef struct RHI_Sampler_Impl RHI_Sampler_Impl;
typedef struct { 
    void (*destroy)(RHI_Sampler_Impl*);
} RHI_Sampler_Vtable;

struct RHI_Sampler_Impl { 
    const RHI_Sampler_Vtable* vtable;
};

RHI_Sampler_Impl** RHI_Sampler_Get_Impl(RHI_Sampler sampler);

#endif