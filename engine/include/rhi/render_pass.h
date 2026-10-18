#ifndef RHI_RENDER_PASS_H
#define RHI_RENDER_PASS_H

#include "rhi/device.h"

typedef struct RHI_Viewport { 
    float x, y, width, height, minDepth, maxDepth;
} RHI_Viewport;

typedef struct RHI_Scissor {
    int32_t x, y;
    uint32_t width, height;
} RHI_Scissor;

typedef struct RHI_RenderPass {
    RHI_Image colorTarget;
    uint32_t colorAttachmentCount;
    RHI_Image colorTargets[8];
    uint32_t resolveAttachmentCount;
    RHI_Image resolveTargets[8];
    bool hasDepthTarget;
    RHI_Image depthTarget;
    bool hasStencilTarget;
    RHI_Image stencilTarget;
    bool clearColor;
    RHI_Color colorClear;
    bool clearDepth;
    float depthClear;
    bool clearStencil;
    uint8_t stencilClear;
    RHI_Viewport viewport;
    RHI_Scissor scissor;
} RHI_RenderPass;

bool RHI_Device_Begin_Render_Pass(RHI_Device device, const RHI_RenderPass* pass);
void RHI_Device_End_Render_Pass(RHI_Device device);

#endif
