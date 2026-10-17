#ifndef RHI_BUFFER_H
#define RHI_BUFFER_H
#include "rhi/handles.h"
#include <stddef.h>
#include <stdint.h>

typedef enum RHI_BufferUsage { 
    RHI_BUFFER_VERTEX       = 1 << 0,
    RHI_BUFFER_INDEX        = 1 << 1, 
    RHI_BUFFER_UNIFORM      = 1 << 2, 
    RHI_BUFFER_STORAGE      = 1 << 3,
    RHI_BUFFER_TRANSFER_SRC = 1 << 4,
    RHI_BUFFER_TRANSFER_DST = 1 << 5
} RHI_BufferUsage;

typedef struct RHI_BufferDesc { 
    size_t size; 
    uint32_t usage; 
    const void* initialData; 
} RHI_BufferDesc;

RHI_Buffer RHI_Buffer_Create(RHI_Instance instance, RHI_Device device, const RHI_BufferDesc* desc);
void RHI_Buffer_Destroy(RHI_Buffer buffer);
size_t RHI_Buffer_Get_Size(RHI_Buffer buffer);
#endif
