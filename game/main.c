#include "rhi/image.h"
#include "rhi/instance.h"
#include "rhi/device.h"
#include "rhi/swapchain.h"
#include "rhi/pipeline.h"
#include "rhi/render_pass.h"
#include "rhi/buffer.h"
#include "rhi/sampler.h"
#include "window/window.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

typedef struct Vertex {
    float position[2];
    float color[3];
} Vertex;

typedef struct Transform {
    float offset[2];
    float rotation;
    float _pad;
} Transform;

typedef struct DemoTargets {
    RHI_Device device;
    RHI_Swapchain swapchain;
    RHI_Image multisampled;
    RHI_Image resolved;
} DemoTargets;

static void OnWindowResize(void* userData, uint32_t width, uint32_t height) {
    DemoTargets* targets = (DemoTargets*)userData;
    if (!RHI_Swapchain_Resize(targets->swapchain, width, height)) {
        printf("Swapchain resize failed\n");
        return;
    }
    if (!width || !height) return;
    if (!RHI_Image_Resize(targets->device, targets->multisampled, width, height) ||
        !RHI_Image_Resize(targets->device, targets->resolved, width, height)) {
        printf("Offscreen target resize failed\n");
    }
}

static bool ReadWholeFile(const char* path, void** outData, size_t* outSize) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long size = ftell(f);
    if (size < 0) { fclose(f); return false; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return false; }

    void* data = malloc((size_t)size);
    if (!data) { fclose(f); return false; }

    size_t readBytes = fread(data, 1, (size_t)size, f);
    fclose(f);
    if (readBytes != (size_t)size) {
        free(data);
        return false;
    }

    *outData = data;
    *outSize = (size_t)size;
    return true;
}

int main() {
    Window* window = Window_Create(&(WindowDesc){
        .title = "4x MSAA Triangle",
        .width = 1280,
        .height = 720,
        .resizable = true,
    });
    if (!window) {
        printf("Failed to create window\n");
        return 1;
    }

    RHI_Instance instance = RHI_Instance_Invalid();
    RHI_Device device = RHI_Device_Invalid();
    RHI_Swapchain swapchain = RHI_Swapchain_Invalid();
    RHI_Image multisampledImage = RHI_Image_Invalid();
    RHI_Image resolvedImage = RHI_Image_Invalid();
    RHI_Buffer vertexBuffer = RHI_Buffer_Invalid();
    RHI_Buffer indexBuffer = RHI_Buffer_Invalid();
    RHI_Buffer vertexStaging = RHI_Buffer_Invalid();
    RHI_Buffer indexStaging = RHI_Buffer_Invalid();
    RHI_PipelineLayout triangleLayout = RHI_PipelineLayout_Invalid();
    RHI_PipelineLayout fullscreenLayout = RHI_PipelineLayout_Invalid();
    RHI_Pipeline trianglePipeline = RHI_Pipeline_Invalid();
    RHI_Pipeline fullscreenPipeline = RHI_Pipeline_Invalid();
    RHI_Sampler fullscreenSampler = RHI_Sampler_Invalid();

    instance = RHI_Instance_Create(RHI_BACKEND_AUTO, true);
    if (!RHI_Instance_IsValid(instance)) {
        printf("Failed to create RHI instance\n");
        goto cleanup;
    }

    device = RHI_Device_Create(instance);
    if (!RHI_Device_IsValid(device)) {
        printf("Failed to create RHI device\n");
        goto cleanup;
    }

    swapchain = RHI_Swapchain_Create(instance, device, window);
    if (!RHI_Swapchain_IsValid(swapchain)) {
        printf("Failed to create RHI swapchain\n");
        goto cleanup;
    }

    RHI_Image swapchainImage = RHI_Swapchain_Get_Current_Image(swapchain);
    RHI_ImageInfo swapchainInfo = RHI_Image_Get_Info(swapchainImage);
    multisampledImage = RHI_Image_Create(instance, device, &(RHI_ImageDesc){
        .width = swapchainInfo.width,
        .height = swapchainInfo.height,
        .format = RHI_IMAGE_FORMAT_RGBA8_UNORM,
        .renderTarget = true,
        .sampleCount = 4,
    });
    resolvedImage = RHI_Image_Create(instance, device, &(RHI_ImageDesc){
        .width = swapchainInfo.width,
        .height = swapchainInfo.height,
        .format = RHI_IMAGE_FORMAT_RGBA8_UNORM,
        .renderTarget = true,
        .sampled = true,
        .sampleCount = 1,
    });
    if (!RHI_Image_IsValid(multisampledImage) || !RHI_Image_IsValid(resolvedImage)) {
        printf("Failed to create 4x MSAA and resolve images\n");
        goto cleanup;
    }

    const Vertex vertices[] = {
        {{ 0.0f,  0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
    };
    const uint16_t indices[] = {0, 1, 2};
    vertexBuffer = RHI_Buffer_Create(instance, device, &(RHI_BufferDesc){
        .size = sizeof(vertices),
        .usage = RHI_BUFFER_VERTEX | RHI_BUFFER_TRANSFER_DST,
    });
    indexBuffer = RHI_Buffer_Create(instance, device, &(RHI_BufferDesc){
        .size = sizeof(indices),
        .usage = RHI_BUFFER_INDEX | RHI_BUFFER_TRANSFER_DST,
    });
    vertexStaging = RHI_Buffer_Create(instance, device, &(RHI_BufferDesc){
        .size = sizeof(vertices),
        .usage = RHI_BUFFER_TRANSFER_SRC,
    });
    indexStaging = RHI_Buffer_Create(instance, device, &(RHI_BufferDesc){
        .size = sizeof(indices),
        .usage = RHI_BUFFER_TRANSFER_SRC,
    });
    if (!RHI_Buffer_IsValid(vertexBuffer) || !RHI_Buffer_IsValid(indexBuffer) ||
        !RHI_Buffer_IsValid(vertexStaging) || !RHI_Buffer_IsValid(indexStaging) ||
        !RHI_Buffer_Upload(instance, device, vertexStaging, 0, sizeof(vertices), vertices) ||
        !RHI_Buffer_Upload(instance, device, indexStaging, 0, sizeof(indices), indices) ||
        !RHI_Buffer_Copy(instance, device, vertexBuffer, 0, vertexStaging, 0, sizeof(vertices)) ||
        !RHI_Buffer_Copy(instance, device, indexBuffer, 0, indexStaging, 0, sizeof(indices))) {
        printf("Failed to create or upload triangle buffers\n");
        goto cleanup;
    }
    RHI_Buffer_Destroy(vertexStaging);
    vertexStaging = RHI_Buffer_Invalid();
    RHI_Buffer_Destroy(indexStaging);
    indexStaging = RHI_Buffer_Invalid();

    triangleLayout = RHI_PipelineLayout_Create(instance, device, &(RHI_PipelineLayoutDesc){
        .pushConstantSize = sizeof(Transform),
        .pushConstantStages = 1u << RHI_SHADER_STAGE_VERTEX,
    });
    fullscreenLayout = RHI_PipelineLayout_Create(instance, device, &(RHI_PipelineLayoutDesc){
        .bindings = (RHI_PipelineBinding[]){
            {0, RHI_BINDING_SAMPLED_IMAGE, 1u << RHI_SHADER_STAGE_FRAGMENT},
            {0, RHI_BINDING_SAMPLER, 1u << RHI_SHADER_STAGE_FRAGMENT},
        },
        .bindingCount = 2,
    });
    fullscreenSampler = RHI_Sampler_Create(instance, device, &(RHI_SamplerDesc){
        .minFilter = RHI_FILTER_LINEAR,
        .magFilter = RHI_FILTER_LINEAR,
        .mipFilter = RHI_FILTER_NEAREST,
        .addressU = RHI_ADDRESS_CLAMP,
        .addressV = RHI_ADDRESS_CLAMP,
        .addressW = RHI_ADDRESS_CLAMP,
        .maxAnisotropy = 1,
        .maxLod = 0.0f,
    });
    if (!RHI_PipelineLayout_IsValid(triangleLayout) ||
        !RHI_PipelineLayout_IsValid(fullscreenLayout) ||
        !RHI_Sampler_IsValid(fullscreenSampler)) {
        printf("Failed to create pipeline layouts or sampler\n");
        goto cleanup;
    }

    void* triangleVs = NULL;
    void* trianglePs = NULL;
    void* fullscreenVs = NULL;
    void* fullscreenPs = NULL;
    size_t triangleVsSize = 0;
    size_t trianglePsSize = 0;
    size_t fullscreenVsSize = 0;
    size_t fullscreenPsSize = 0;
    bool shadersLoaded =
        ReadWholeFile("build/shaders/triangle_vertexMain.dxil", &triangleVs, &triangleVsSize) &&
        ReadWholeFile("build/shaders/triangle_fragmentMain.dxil", &trianglePs, &trianglePsSize) &&
        ReadWholeFile("build/shaders/fullscreen_vertexMain.dxil", &fullscreenVs, &fullscreenVsSize) &&
        ReadWholeFile("build/shaders/fullscreen_fragmentMain.dxil", &fullscreenPs, &fullscreenPsSize);
    if (!shadersLoaded) {
        printf("Failed to read compiled shaders (run build_shaders.py)\n");
        free(triangleVs);
        free(trianglePs);
        free(fullscreenVs);
        free(fullscreenPs);
        goto cleanup;
    }

    RHI_ImageInfo msaaInfo = RHI_Image_Get_Info(multisampledImage);
    trianglePipeline = RHI_Pipeline_Create(instance, device, &(RHI_PipelineDesc){
        .layout = triangleLayout,
        .stages = (RHI_ShaderStageDesc[]){
            {RHI_SHADER_STAGE_VERTEX, triangleVs, triangleVsSize},
            {RHI_SHADER_STAGE_FRAGMENT, trianglePs, trianglePsSize},
        },
        .stageCount = 2,
        .topology = RHI_TOPOLOGY_TRIANGLE_LIST,
        .cullMode = RHI_CULL_BACK,
        .frontFace = RHI_FRONT_FACE_CLOCKWISE,
        .colorAttachmentCount = 1,
        .colorFormats = {msaaInfo.format},
        .sampleCount = 4,
        .vertexAttributes = (RHI_VertexAttribute[]){
            {"POSITION", 0, 0, offsetof(Vertex, position), RHI_VERTEX_FORMAT_FLOAT2, false},
            {"COLOR", 0, 0, offsetof(Vertex, color), RHI_VERTEX_FORMAT_FLOAT3, false},
        },
        .vertexAttributeCount = 2,
    });
    fullscreenPipeline = RHI_Pipeline_Create(instance, device, &(RHI_PipelineDesc){
        .layout = fullscreenLayout,
        .stages = (RHI_ShaderStageDesc[]){
            {RHI_SHADER_STAGE_VERTEX, fullscreenVs, fullscreenVsSize},
            {RHI_SHADER_STAGE_FRAGMENT, fullscreenPs, fullscreenPsSize},
        },
        .stageCount = 2,
        .topology = RHI_TOPOLOGY_TRIANGLE_LIST,
        .cullMode = RHI_CULL_NONE,
        .colorAttachmentCount = 1,
        .colorFormats = {swapchainInfo.format},
        .sampleCount = 1,
    });
    free(triangleVs);
    free(trianglePs);
    free(fullscreenVs);
    free(fullscreenPs);
    if (!RHI_Pipeline_IsValid(trianglePipeline) || !RHI_Pipeline_IsValid(fullscreenPipeline)) {
        printf("Failed to create render pipelines\n");
        goto cleanup;
    }

    DemoTargets targets = {device, swapchain, multisampledImage, resolvedImage};
    Window_Set_Resize_Callback(window, OnWindowResize, &targets);
    printf("Success!\n");

    clock_t startTime = clock();
    while (!Window_Should_Close(window)) {
        Window_Poll_Events();
        if (Window_Should_Close(window)) break;

        float t = (float)(clock() - startTime) / (float)CLOCKS_PER_SEC;
        Transform transform = {
            .offset = {0.3f * cosf(t), 0.3f * sinf(t)},
            .rotation = t,
        };
        uint32_t frameIndex = RHI_Swapchain_Acquire_Next_Image(swapchain);
        if (RHI_Device_Begin_Frame(device, frameIndex)) {
            RHI_ImageInfo offscreenInfo = RHI_Image_Get_Info(multisampledImage);
            RHI_Device_Begin_Render_Pass(device, &(RHI_RenderPass){
                .colorTarget = multisampledImage,
                .colorAttachmentCount = 1,
                .colorTargets = {multisampledImage},
                .resolveAttachmentCount = 1,
                .resolveTargets = {resolvedImage},
                .clearColor = true,
                .colorClear = {0.05f, 0.05f, 0.08f, 1.0f},
                .viewport = {0, 0, (float)offscreenInfo.width, (float)offscreenInfo.height, 0, 1},
                .scissor = {0, 0, offscreenInfo.width, offscreenInfo.height},
            });
            RHI_Device_Bind_Pipeline(device, trianglePipeline);
            RHI_Device_Bind_Push_Constants(device, 0, sizeof(transform), &transform);
            RHI_Device_Bind_Vertex_Buffer(device, 0, vertexBuffer, sizeof(Vertex));
            RHI_Device_Bind_Index_Buffer(device, indexBuffer, false);
            RHI_Device_Draw_Indexed(device, 3, 1, 0, 0, 0);
            RHI_Device_End_Render_Pass(device);

            RHI_Image backbuffer = RHI_Swapchain_Get_Current_Image(swapchain);
            RHI_ImageInfo backbufferInfo = RHI_Image_Get_Info(backbuffer);
            RHI_Device_Begin_Render_Pass(device, &(RHI_RenderPass){
                .colorTarget = backbuffer,
                .clearColor = true,
                .colorClear = {0, 0, 0, 1},
                .viewport = {0, 0, (float)backbufferInfo.width, (float)backbufferInfo.height, 0, 1},
                .scissor = {0, 0, backbufferInfo.width, backbufferInfo.height},
            });
            RHI_Device_Bind_Pipeline(device, fullscreenPipeline);
            RHI_Device_Bind_Image(device, 0, resolvedImage);
            RHI_Device_Bind_Sampler(device, 0, fullscreenSampler);
            RHI_Device_Draw(device, 3, 1, 0, 0);
            RHI_Device_End_Render_Pass(device);
            RHI_Device_End_Frame(device);
        }
        RHI_Swapchain_Present(swapchain);
    }

cleanup:
    if (RHI_Pipeline_IsValid(fullscreenPipeline)) RHI_Pipeline_Destroy(fullscreenPipeline);
    if (RHI_Pipeline_IsValid(trianglePipeline)) RHI_Pipeline_Destroy(trianglePipeline);
    if (RHI_Sampler_IsValid(fullscreenSampler)) RHI_Sampler_Destroy(fullscreenSampler);
    if (RHI_PipelineLayout_IsValid(fullscreenLayout)) RHI_PipelineLayout_Destroy(fullscreenLayout);
    if (RHI_PipelineLayout_IsValid(triangleLayout)) RHI_PipelineLayout_Destroy(triangleLayout);
    if (RHI_Buffer_IsValid(vertexStaging)) RHI_Buffer_Destroy(vertexStaging);
    if (RHI_Buffer_IsValid(indexStaging)) RHI_Buffer_Destroy(indexStaging);
    if (RHI_Buffer_IsValid(indexBuffer)) RHI_Buffer_Destroy(indexBuffer);
    if (RHI_Buffer_IsValid(vertexBuffer)) RHI_Buffer_Destroy(vertexBuffer);
    if (RHI_Image_IsValid(resolvedImage)) RHI_Image_Destroy(resolvedImage);
    if (RHI_Image_IsValid(multisampledImage)) RHI_Image_Destroy(multisampledImage);
    if (RHI_Swapchain_IsValid(swapchain)) RHI_Swapchain_Destroy(swapchain);
    if (RHI_Device_IsValid(device)) RHI_Device_Destroy(device);
    if (RHI_Instance_IsValid(instance)) RHI_Instance_Destroy(instance);
    Window_Destroy(window);
    return 0;
}
