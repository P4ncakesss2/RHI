#ifndef INSTANCE_H_INTERNAL
#define INSTANCE_H_INTERNAL

#include "rhi/instance.h"
typedef struct RHI_Instance_Impl RHI_Instance_Impl;
typedef struct {
    void (*destroy)(RHI_Instance_Impl* self);
} RHI_Instance_Vtable;

struct RHI_Instance_Impl {
    const RHI_Instance_Vtable* vtable;
    RHI_Backend backend;
};

RHI_Instance_Impl** RHI_Instance_Get_Impl(RHI_Instance handle);

#endif