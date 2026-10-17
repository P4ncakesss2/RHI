#ifndef DEVICE_H
#define DEVICE_H

#include "rhi/handles.h"
#include <stdbool.h>
#include <stdint.h>

RHI_Device RHI_Device_Create(RHI_Instance instance);
void RHI_Device_Destroy(RHI_Device handle);

bool RHI_Device_Begin_Frame(RHI_Device device, uint32_t frameIndex);
void RHI_Device_End_Frame(RHI_Device device);

typedef struct { float r, g, b, a; } RHI_Color;
void RHI_Device_Bind_Pipeline(RHI_Device device, RHI_Pipeline pipeline);
void RHI_Device_Draw(RHI_Device device, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
void RHI_Device_Dispatch(RHI_Device device, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
bool RHI_Device_Bind_Vertex_Buffer(RHI_Device device, uint32_t slot, RHI_Buffer buffer, uint32_t stride);
bool RHI_Device_Bind_Index_Buffer(RHI_Device device, RHI_Buffer buffer, bool index32Bit);
void RHI_Device_Draw_Indexed(RHI_Device device, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

#endif
