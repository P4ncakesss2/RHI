#ifndef RHI_PIPELINE_H
#define RHI_PIPELINE_H

#include "rhi/handles.h"
#include "rhi/image.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum RHI_BindingType {
    RHI_BINDING_UNIFORM_BUFFER,
    RHI_BINDING_STORAGE_BUFFER,
    RHI_BINDING_SAMPLED_IMAGE,
    RHI_BINDING_SAMPLER,
    RHI_BINDING_STORAGE_BUFFER_UAV,
    RHI_BINDING_STORAGE_IMAGE,
} RHI_BindingType;

typedef uint32_t RHI_ShaderStageMask;

#define RHI_PIPELINE_MAX_BINDINGS 16

typedef struct {
    uint32_t binding;
    RHI_BindingType type;
    RHI_ShaderStageMask stageMask;
} RHI_PipelineBinding;

typedef struct {
    const RHI_PipelineBinding *bindings;
    uint32_t bindingCount;
    uint32_t pushConstantSize;
    RHI_ShaderStageMask pushConstantStages;
} RHI_PipelineLayoutDesc;

RHI_PipelineLayout RHI_PipelineLayout_Create(RHI_Instance instance, RHI_Device device, const RHI_PipelineLayoutDesc *desc);
void RHI_PipelineLayout_Destroy(RHI_PipelineLayout handle);

typedef enum RHI_ShaderStage {
    RHI_SHADER_STAGE_VERTEX,
    RHI_SHADER_STAGE_TESSELLATION_CONTROL,
    RHI_SHADER_STAGE_TESSELLATION_EVALUATION,
    RHI_SHADER_STAGE_GEOMETRY,
    RHI_SHADER_STAGE_FRAGMENT,
    RHI_SHADER_STAGE_COMPUTE,
} RHI_ShaderStage;

typedef struct {
    RHI_ShaderStage stage;
    const void *code;
    size_t codeSize;
} RHI_ShaderStageDesc;

#define RHI_PIPELINE_MAX_COLOR_ATTACHMENTS 8
#define RHI_PIPELINE_MAX_VERTEX_ATTRIBUTES 16

typedef enum RHI_PrimitiveTopology {
    RHI_TOPOLOGY_TRIANGLE_LIST,
    RHI_TOPOLOGY_TRIANGLE_STRIP,
    RHI_TOPOLOGY_LINE_LIST,
    RHI_TOPOLOGY_LINE_STRIP,
    RHI_TOPOLOGY_POINT_LIST
} RHI_PrimitiveTopology;
typedef enum RHI_CullMode {
    RHI_CULL_NONE,
    RHI_CULL_FRONT,
    RHI_CULL_BACK
} RHI_CullMode;
typedef enum RHI_FrontFace {
    RHI_FRONT_FACE_CLOCKWISE,
    RHI_FRONT_FACE_COUNTER_CLOCKWISE
} RHI_FrontFace;
typedef enum RHI_VertexFormat {
    RHI_VERTEX_FORMAT_FLOAT,
    RHI_VERTEX_FORMAT_FLOAT2,
    RHI_VERTEX_FORMAT_FLOAT3,
    RHI_VERTEX_FORMAT_FLOAT4,
    RHI_VERTEX_FORMAT_INT,
    RHI_VERTEX_FORMAT_INT2,
    RHI_VERTEX_FORMAT_INT3,
    RHI_VERTEX_FORMAT_INT4,
    RHI_VERTEX_FORMAT_UINT,
    RHI_VERTEX_FORMAT_UINT2,
    RHI_VERTEX_FORMAT_UINT3,
    RHI_VERTEX_FORMAT_UINT4,
    RHI_VERTEX_FORMAT_UNORM8,
    RHI_VERTEX_FORMAT_SNORM16
} RHI_VertexFormat;
typedef enum RHI_BlendFactor {
    RHI_BLEND_ZERO,
    RHI_BLEND_ONE,
    RHI_BLEND_SRC_COLOR,
    RHI_BLEND_ONE_MINUS_SRC_COLOR,
    RHI_BLEND_SRC_ALPHA,
    RHI_BLEND_ONE_MINUS_SRC_ALPHA,
    RHI_BLEND_DST_COLOR,
    RHI_BLEND_ONE_MINUS_DST_COLOR,
    RHI_BLEND_DST_ALPHA,
    RHI_BLEND_ONE_MINUS_DST_ALPHA
} RHI_BlendFactor;
typedef enum RHI_BlendOp {
    RHI_BLEND_ADD,
    RHI_BLEND_SUBTRACT,
    RHI_BLEND_REVERSE_SUBTRACT,
    RHI_BLEND_MIN,
    RHI_BLEND_MAX
} RHI_BlendOp;
typedef enum RHI_CompareOp {
    RHI_COMPARE_NEVER,
    RHI_COMPARE_LESS,
    RHI_COMPARE_EQUAL,
    RHI_COMPARE_LESS_EQUAL,
    RHI_COMPARE_GREATER,
    RHI_COMPARE_NOT_EQUAL,
    RHI_COMPARE_GREATER_EQUAL,
    RHI_COMPARE_ALWAYS
} RHI_CompareOp;

typedef struct {
    const char *semantic;
    uint32_t semanticIndex, inputSlot, offset;
    RHI_VertexFormat format;
    bool perInstance;
} RHI_VertexAttribute;

typedef struct {
    RHI_BlendFactor srcColor, dstColor, srcAlpha, dstAlpha;
    RHI_BlendOp colorOp, alphaOp;
    bool enabled;
} RHI_BlendState;

typedef struct {
    bool depthTest, depthWrite, stencilTest;
    RHI_CompareOp depthCompare;
} RHI_DepthStencilState;

typedef struct {
    RHI_PipelineLayout layout;
    const RHI_ShaderStageDesc *stages;
    uint32_t stageCount;
    RHI_PrimitiveTopology topology;
    RHI_CullMode cullMode;
    RHI_FrontFace frontFace;
    bool wireframe;
    const RHI_VertexAttribute *vertexAttributes;
    uint32_t vertexAttributeCount;
    RHI_BlendState blend;
    RHI_DepthStencilState depthStencil;
    uint32_t colorAttachmentCount;
    RHI_ImageFormat colorFormats[RHI_PIPELINE_MAX_COLOR_ATTACHMENTS];
    RHI_ImageFormat depthFormat;
    uint32_t sampleCount;
} RHI_PipelineDesc;

RHI_Pipeline RHI_Pipeline_Create(RHI_Instance instance, RHI_Device device,
                                 const RHI_PipelineDesc *desc);
void RHI_Pipeline_Destroy(RHI_Pipeline handle);

#endif
