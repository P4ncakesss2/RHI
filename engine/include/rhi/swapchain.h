#ifndef RHI_SWAPCHAIN_H
#define RHI_SWAPCHAIN_H

#include "rhi/handles.h"
#include "window/window.h"
#include <stdint.h>

RHI_Swapchain RHI_Swapchain_Create(RHI_Instance instance, RHI_Device device, Window* window);
void RHI_Swapchain_Destroy(RHI_Swapchain handle);
bool RHI_Swapchain_Resize(RHI_Swapchain handle, uint32_t width, uint32_t height);

uint32_t RHI_Swapchain_Acquire_Next_Image(RHI_Swapchain handle);
RHI_Image RHI_Swapchain_Get_Current_Image(RHI_Swapchain handle);
void RHI_Swapchain_Present(RHI_Swapchain handle);

#endif
