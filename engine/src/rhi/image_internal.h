#ifndef RHI_IMAGE_INTERNAL_H
#define RHI_IMAGE_INTERNAL_H
#include "rhi/image.h"
#include <stdbool.h>
#include <dxgi1_6.h>

typedef struct RHI_Image_Impl RHI_Image_Impl;
typedef struct {
    void (*destroy)(RHI_Image_Impl* self);
} RHI_Image_Vtable;

struct RHI_Image_Impl { 
    const RHI_Image_Vtable* vtable;
    RHI_ImageInfo info;
};

RHI_Image RHI_Image_Register(RHI_Image_Impl* impl);
void RHI_Image_Unregister(RHI_Image image);

RHI_Image_Impl** RHI_Image_Get_Impl(RHI_Image image);

#endif
