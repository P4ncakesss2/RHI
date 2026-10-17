#ifndef PIPELINE_H_INTERNAL
#define PIPELINE_H_INTERNAL

#include "rhi/handles.h"

typedef struct RHI_PipelineLayout_Impl RHI_PipelineLayout_Impl;
typedef struct {
    void (*destroy)(RHI_PipelineLayout_Impl* self);
} RHI_PipelineLayout_Vtable;

struct RHI_PipelineLayout_Impl {
    const RHI_PipelineLayout_Vtable* vtable;
};

typedef struct RHI_Pipeline_Impl RHI_Pipeline_Impl;
typedef struct {
    void (*destroy)(RHI_Pipeline_Impl* self);
} RHI_Pipeline_Vtable;

struct RHI_Pipeline_Impl {
    const RHI_Pipeline_Vtable* vtable;
};

RHI_PipelineLayout_Impl** RHI_PipelineLayout_Get_Impl(RHI_PipelineLayout handle);
RHI_Pipeline_Impl** RHI_Pipeline_Get_Impl(RHI_Pipeline handle);

#endif