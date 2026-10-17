#ifndef RHI_SAMPLER_H
#define RHI_SAMPLER_H
#include "rhi/handles.h"

typedef enum RHI_Filter {
    RHI_FILTER_NEAREST,
    RHI_FILTER_LINEAR,
} RHI_Filter;

typedef enum RHI_AddressMode {
    RHI_ADDRESS_REPEAT,
    RHI_ADDRESS_MIRRORED_REPEAT,
    RHI_ADDRESS_CLAMP,
    RHI_ADDRESS_CLAMP_TO_BORDER,
    RHI_ADDRESS_MIRROR_CLAMP,
} RHI_AddressMode;

typedef enum RHI_SamplerCompareOp {
    RHI_SAMPLER_COMPARE_NEVER,
    RHI_SAMPLER_COMPARE_LESS,
    RHI_SAMPLER_COMPARE_EQUAL,
    RHI_SAMPLER_COMPARE_LESS_EQUAL,
    RHI_SAMPLER_COMPARE_GREATER,
    RHI_SAMPLER_COMPARE_NOT_EQUAL,
    RHI_SAMPLER_COMPARE_GREATER_EQUAL,
    RHI_SAMPLER_COMPARE_ALWAYS,
} RHI_SamplerCompareOp;

typedef enum RHI_SamplerBorderColor {
    RHI_BORDER_TRANSPARENT_BLACK,
    RHI_BORDER_OPAQUE_BLACK,
    RHI_BORDER_OPAQUE_WHITE,
} RHI_SamplerBorderColor;

typedef struct RHI_SamplerDesc {
    RHI_Filter minFilter;
    RHI_Filter magFilter;
    RHI_Filter mipFilter;
    RHI_AddressMode addressU;
    RHI_AddressMode addressV;
    RHI_AddressMode addressW;
    bool anisotropyEnable;
    uint32_t maxAnisotropy;
    bool compareEnable;
    RHI_SamplerCompareOp compareOp;
    RHI_SamplerBorderColor borderColor;
    float lodBias;
    float minLod;
    float maxLod;
    } RHI_SamplerDesc;
RHI_Sampler RHI_Sampler_Create(RHI_Instance instance, RHI_Device device,
                               const RHI_SamplerDesc *desc);
void RHI_Sampler_Destroy(RHI_Sampler sampler);

#endif
