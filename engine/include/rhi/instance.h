#ifndef INSTANCE_H
#define INSTANCE_H

#include "rhi/handles.h"
#include <stdbool.h>

typedef enum RHI_Backend {
    RHI_BACKEND_DX12,
    RHI_BACKEND_VULKAN,
    RHI_BACKEND_AUTO
} RHI_Backend;

RHI_Instance RHI_Instance_Create(RHI_Backend backend, bool debug);
void RHI_Instance_Destroy(RHI_Instance handle);
RHI_Backend RHI_Instance_Get_Backend(RHI_Instance handle);

#endif
