#include "dx12.h"
#include "rhi/device_internal.h"
#include "rhi/pipeline_internal.h"
#include "rhi/image_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <d3d12.h>

static void DX12_PipelineLayout_Destroy(RHI_PipelineLayout_Impl* self) {
    DX12PipelineLayout* layout = (DX12PipelineLayout*)self;
    if (layout->rootSignature) {
        layout->rootSignature->lpVtbl->Release(layout->rootSignature);
    }
    free(layout);
}

static const RHI_PipelineLayout_Vtable dx12PipelineLayoutVtable = {
    .destroy = DX12_PipelineLayout_Destroy,
};

bool DX12_PipelineLayout_Create(RHI_Device device, const RHI_PipelineLayoutDesc* desc, RHI_PipelineLayout_Impl** outLayout) {
    (void)desc; // TODO

    RHI_Device_Impl** device_impl = RHI_Device_Get_Impl(device);
    if (!device_impl || !*device_impl) {
        return false;
    }
    DX12Device* dxDevice = (DX12Device*)*device_impl;

    DX12PipelineLayout* layout = calloc(1, sizeof(DX12PipelineLayout));
    if (!layout) {
        return false;
    }
    layout->base.vtable = &dx12PipelineLayoutVtable;

    D3D12_ROOT_SIGNATURE_DESC rootDesc = {0};
    rootDesc.NumParameters = 0;
    rootDesc.pParameters = NULL;
    rootDesc.NumStaticSamplers = 0;
    rootDesc.pStaticSamplers = NULL;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob* signatureBlob = NULL;
    ID3DBlob* errorBlob = NULL;
    HRESULT hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                            &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            fprintf(stderr, "Root signature serialization failed: %s\n",
                    (const char*)errorBlob->lpVtbl->GetBufferPointer(errorBlob));
            errorBlob->lpVtbl->Release(errorBlob);
        }
        free(layout);
        return false;
    }
    if (errorBlob) {
        errorBlob->lpVtbl->Release(errorBlob);
    }

    hr = dxDevice->device->lpVtbl->CreateRootSignature(
        dxDevice->device, 0,
        signatureBlob->lpVtbl->GetBufferPointer(signatureBlob),
        signatureBlob->lpVtbl->GetBufferSize(signatureBlob),
        &IID_ID3D12RootSignature, (void**)&layout->rootSignature);
    signatureBlob->lpVtbl->Release(signatureBlob);

    if (FAILED(hr)) {
        free(layout);
        return false;
    }

    *outLayout = &layout->base;
    return true;
}

static void DX12_Pipeline_Destroy(RHI_Pipeline_Impl* self) {
    DX12Pipeline* pipeline = (DX12Pipeline*)self;
    if (pipeline->pso) {
        pipeline->pso->lpVtbl->Release(pipeline->pso);
    }
    free(pipeline);
}

static const RHI_Pipeline_Vtable dx12PipelineVtable = {
    .destroy = DX12_Pipeline_Destroy,
};

static const RHI_ShaderStageDesc* DX12_FindStage(const RHI_PipelineDesc* desc, RHI_ShaderStage stage) {
    for (uint32_t i = 0; i < desc->stageCount; ++i) {
        if (desc->stages[i].stage == stage) {
            return &desc->stages[i];
        }
    }
    return NULL;
}

static DXGI_FORMAT DX12_VertexFormat(RHI_VertexFormat f) {
    switch (f) {
        case RHI_VERTEX_FORMAT_FLOAT:   return DXGI_FORMAT_R32_FLOAT;
        case RHI_VERTEX_FORMAT_FLOAT2:  return DXGI_FORMAT_R32G32_FLOAT;
        case RHI_VERTEX_FORMAT_FLOAT3:  return DXGI_FORMAT_R32G32B32_FLOAT;
        case RHI_VERTEX_FORMAT_FLOAT4:  return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case RHI_VERTEX_FORMAT_INT:     return DXGI_FORMAT_R32_SINT;
        case RHI_VERTEX_FORMAT_INT2:    return DXGI_FORMAT_R32G32_SINT;
        case RHI_VERTEX_FORMAT_INT3:    return DXGI_FORMAT_R32G32B32_SINT;
        case RHI_VERTEX_FORMAT_INT4:    return DXGI_FORMAT_R32G32B32A32_SINT;
        case RHI_VERTEX_FORMAT_UINT:    return DXGI_FORMAT_R32_UINT;
        case RHI_VERTEX_FORMAT_UINT2:   return DXGI_FORMAT_R32G32_UINT;
        case RHI_VERTEX_FORMAT_UINT3:   return DXGI_FORMAT_R32G32B32_UINT;
        case RHI_VERTEX_FORMAT_UINT4:   return DXGI_FORMAT_R32G32B32A32_UINT;
        case RHI_VERTEX_FORMAT_UNORM8:  return DXGI_FORMAT_R8G8B8A8_UNORM;
        case RHI_VERTEX_FORMAT_SNORM16: return DXGI_FORMAT_R16G16_SNORM;
        default:                        return DXGI_FORMAT_UNKNOWN;
    }
}

static D3D12_BLEND DX12_BlendFactor(RHI_BlendFactor f) {
    switch (f) {
        case RHI_BLEND_ZERO:                return D3D12_BLEND_ZERO;
        case RHI_BLEND_ONE:                 return D3D12_BLEND_ONE;
        case RHI_BLEND_SRC_COLOR:           return D3D12_BLEND_SRC_COLOR;
        case RHI_BLEND_ONE_MINUS_SRC_COLOR: return D3D12_BLEND_INV_SRC_COLOR;
        case RHI_BLEND_SRC_ALPHA:           return D3D12_BLEND_SRC_ALPHA;
        case RHI_BLEND_ONE_MINUS_SRC_ALPHA: return D3D12_BLEND_INV_SRC_ALPHA;
        case RHI_BLEND_DST_COLOR:           return D3D12_BLEND_DEST_COLOR;
        case RHI_BLEND_ONE_MINUS_DST_COLOR: return D3D12_BLEND_INV_DEST_COLOR;
        case RHI_BLEND_DST_ALPHA:           return D3D12_BLEND_DEST_ALPHA;
        case RHI_BLEND_ONE_MINUS_DST_ALPHA: return D3D12_BLEND_INV_DEST_ALPHA;
        default:                            return D3D12_BLEND_ONE;
    }
}

static D3D12_BLEND_OP DX12_BlendOp(RHI_BlendOp op) {
    switch (op) {
        case RHI_BLEND_SUBTRACT:         return D3D12_BLEND_OP_SUBTRACT;
        case RHI_BLEND_REVERSE_SUBTRACT: return D3D12_BLEND_OP_REV_SUBTRACT;
        case RHI_BLEND_MIN:              return D3D12_BLEND_OP_MIN;
        case RHI_BLEND_MAX:              return D3D12_BLEND_OP_MAX;
        default:                         return D3D12_BLEND_OP_ADD;
    }
}

static D3D12_COMPARISON_FUNC DX12_Compare(RHI_CompareOp op) {
    static const D3D12_COMPARISON_FUNC map[] = {
        D3D12_COMPARISON_FUNC_NEVER,
        D3D12_COMPARISON_FUNC_LESS,
        D3D12_COMPARISON_FUNC_EQUAL,
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_COMPARISON_FUNC_GREATER,
        D3D12_COMPARISON_FUNC_NOT_EQUAL,
        D3D12_COMPARISON_FUNC_GREATER_EQUAL,
        D3D12_COMPARISON_FUNC_ALWAYS
    };
    return op <= RHI_COMPARE_ALWAYS ? map[op] : D3D12_COMPARISON_FUNC_LESS;
}

static DXGI_FORMAT DX12_ImageFormat(RHI_ImageFormat f) {
    return DX12_ImageFormat_To_DXGI(f);
}

bool DX12_Pipeline_Create(RHI_Device device, const RHI_PipelineDesc* desc, RHI_Pipeline_Impl** outPipeline) {
    if (!desc || !outPipeline || !desc->stages || desc->stageCount == 0) {
        return false;
    }
    
    for (uint32_t i = 0; i < desc->stageCount; ++i) {
        if (!desc->stages[i].code || desc->stages[i].codeSize == 0 || desc->stages[i].stage > RHI_SHADER_STAGE_COMPUTE) {
            return false;
        }
    }

    RHI_Device_Impl** device_impl = RHI_Device_Get_Impl(device);
    if (!device_impl || !*device_impl) {
        return false;
    }
    DX12Device* dxDevice = (DX12Device*)*device_impl;

    RHI_PipelineLayout_Impl** layout_impl = RHI_PipelineLayout_Get_Impl(desc->layout);
    if (!layout_impl || !*layout_impl) {
        return false;
    }
    DX12PipelineLayout* dxLayout = (DX12PipelineLayout*)*layout_impl;

    const RHI_ShaderStageDesc* cs = DX12_FindStage(desc, RHI_SHADER_STAGE_COMPUTE);
    if (cs) {
        if (desc->stageCount != 1 || !cs->code || !cs->codeSize) {
            return false;
        }
        
        DX12Pipeline* pipeline = calloc(1, sizeof(*pipeline));
        if (!pipeline) {
            return false;
        }
        
        pipeline->base.vtable = &dx12PipelineVtable;
        pipeline->rootSignature = dxLayout->rootSignature;
        pipeline->isCompute = true;
        
        D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc = {0};
        computeDesc.pRootSignature = dxLayout->rootSignature;
        computeDesc.CS.pShaderBytecode = cs->code;
        computeDesc.CS.BytecodeLength = cs->codeSize;
        
        HRESULT computeHr = dxDevice->device->lpVtbl->CreateComputePipelineState(
            dxDevice->device, &computeDesc, &IID_ID3D12PipelineState, (void**)&pipeline->pso
        );
        if (FAILED(computeHr)) {
            free(pipeline);
            return false;
        }
        
        *outPipeline = &pipeline->base;
        return true;
    }

    const RHI_ShaderStageDesc* vs = DX12_FindStage(desc, RHI_SHADER_STAGE_VERTEX);
    const RHI_ShaderStageDesc* hs = DX12_FindStage(desc, RHI_SHADER_STAGE_TESSELLATION_CONTROL);
    const RHI_ShaderStageDesc* ds = DX12_FindStage(desc, RHI_SHADER_STAGE_TESSELLATION_EVALUATION);
    const RHI_ShaderStageDesc* gs = DX12_FindStage(desc, RHI_SHADER_STAGE_GEOMETRY);
    const RHI_ShaderStageDesc* ps = DX12_FindStage(desc, RHI_SHADER_STAGE_FRAGMENT);
    
    if (!vs || !vs->code || vs->codeSize == 0) {
        fprintf(stderr, "Pipeline is missing a vertex stage\n");
        return false;
    }
    if (!ps || !ps->code || ps->codeSize == 0) {
        fprintf(stderr, "Pipeline is missing a fragment stage\n");
        return false;
    }
    if ((hs == NULL) != (ds == NULL)) {
        return false;
    }
    if (desc->colorAttachmentCount > RHI_PIPELINE_MAX_COLOR_ATTACHMENTS ||
        desc->vertexAttributeCount > RHI_PIPELINE_MAX_VERTEX_ATTRIBUTES ||
        desc->sampleCount == 0) {
        return false;
    }

    DX12Pipeline* pipeline = calloc(1, sizeof(DX12Pipeline));
    if (!pipeline) {
        return false;
    }
    pipeline->base.vtable = &dx12PipelineVtable;
    pipeline->rootSignature = dxLayout->rootSignature;
    pipeline->topology = desc->topology;

    D3D12_RASTERIZER_DESC rasterizer = {0};
    rasterizer.FillMode = desc->wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = desc->cullMode == RHI_CULL_NONE ? D3D12_CULL_MODE_NONE : 
                          (desc->cullMode == RHI_CULL_FRONT ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_BACK);
    rasterizer.FrontCounterClockwise = desc->frontFace == RHI_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_BLEND_DESC blend = {0};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i) {
        D3D12_RENDER_TARGET_BLEND_DESC* rt = &blend.RenderTarget[i];
        rt->BlendEnable = desc->blend.enabled;
        rt->LogicOpEnable = FALSE;
        rt->SrcBlend = DX12_BlendFactor(desc->blend.srcColor);
        rt->DestBlend = DX12_BlendFactor(desc->blend.dstColor);
        rt->BlendOp = DX12_BlendOp(desc->blend.colorOp);
        rt->SrcBlendAlpha = DX12_BlendFactor(desc->blend.srcAlpha);
        rt->DestBlendAlpha = DX12_BlendFactor(desc->blend.dstAlpha);
        rt->BlendOpAlpha = DX12_BlendOp(desc->blend.alphaOp);
        rt->LogicOp = D3D12_LOGIC_OP_NOOP;
        rt->RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    D3D12_DEPTH_STENCIL_DESC depthStencil = {0};
    depthStencil.DepthEnable = desc->depthStencil.depthTest;
    depthStencil.DepthWriteMask = desc->depthStencil.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencil.DepthFunc = DX12_Compare(desc->depthStencil.depthCompare);
    depthStencil.StencilEnable = FALSE;
    depthStencil.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    depthStencil.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    depthStencil.FrontFace.StencilFailOp = depthStencil.FrontFace.StencilDepthFailOp = depthStencil.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depthStencil.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    depthStencil.BackFace = depthStencil.FrontFace;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {0};
    psoDesc.pRootSignature = dxLayout->rootSignature;
    psoDesc.VS.pShaderBytecode = vs->code;
    psoDesc.VS.BytecodeLength = vs->codeSize;
    psoDesc.PS.pShaderBytecode = ps->code;
    psoDesc.PS.BytecodeLength = ps->codeSize;
    if (hs) {
        psoDesc.HS.pShaderBytecode = hs->code;
        psoDesc.HS.BytecodeLength = hs->codeSize;
    }
    if (ds) {
        psoDesc.DS.pShaderBytecode = ds->code;
        psoDesc.DS.BytecodeLength = ds->codeSize;
    }
    if (gs) {
        psoDesc.GS.pShaderBytecode = gs->code;
        psoDesc.GS.BytecodeLength = gs->codeSize;
    }
    psoDesc.BlendState = blend;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = rasterizer;
    psoDesc.DepthStencilState = depthStencil;

    D3D12_INPUT_ELEMENT_DESC inputElements[RHI_PIPELINE_MAX_VERTEX_ATTRIBUTES] = {0};
    for (uint32_t i = 0; i < desc->vertexAttributeCount; ++i) {
        const RHI_VertexAttribute* a = &desc->vertexAttributes[i];
        inputElements[i].SemanticName = a->semantic;
        inputElements[i].SemanticIndex = a->semanticIndex;
        inputElements[i].Format = DX12_VertexFormat(a->format);
        inputElements[i].InputSlot = a->inputSlot;
        inputElements[i].AlignedByteOffset = a->offset;
        inputElements[i].InputSlotClass = a->perInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA 
                                                          : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        if (inputElements[i].Format == DXGI_FORMAT_UNKNOWN || !a->semantic) {
            free(pipeline);
            return false;
        }
    }
    psoDesc.InputLayout.pInputElementDescs = inputElements;
    psoDesc.InputLayout.NumElements = desc->vertexAttributeCount;
    
    switch (desc->topology) {
        case RHI_TOPOLOGY_LINE_LIST:
        case RHI_TOPOLOGY_LINE_STRIP:
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            break;
        case RHI_TOPOLOGY_POINT_LIST:
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            break;
        default:
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            break;
    }
    if (hs) {
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    }
    
    psoDesc.NumRenderTargets = desc->colorAttachmentCount;
    for (uint32_t i = 0; i < desc->colorAttachmentCount; ++i) {
        psoDesc.RTVFormats[i] = DX12_ImageFormat(desc->colorFormats[i]);
    }
    psoDesc.DSVFormat = desc->depthStencil.depthTest ? DX12_ImageFormat(desc->depthFormat) : DXGI_FORMAT_UNKNOWN;
    if (desc->depthStencil.depthTest && psoDesc.DSVFormat == DXGI_FORMAT_UNKNOWN) {
        free(pipeline);
        return false;
    }
    psoDesc.SampleDesc.Count = desc->sampleCount;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.NodeMask = 0;

    HRESULT hr = dxDevice->device->lpVtbl->CreateGraphicsPipelineState(
        dxDevice->device, &psoDesc, &IID_ID3D12PipelineState, (void**)&pipeline->pso
    );

    if (FAILED(hr)) {
        free(pipeline);
        return false;
    }

    *outPipeline = &pipeline->base;
    return true;
}