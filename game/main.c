#include "rhi/image.h"
#include "rhi/instance.h"
#include "rhi/device.h"
#include "rhi/swapchain.h"
#include "rhi/pipeline.h"
#include "rhi/render_pass.h"
#include "rhi/buffer.h"
#include "window/window.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Vertex {
    float position[2];
    float color[3];
} Vertex;

static void OnWindowResize(void* userData, uint32_t width, uint32_t height) {
    RHI_Swapchain swapchain = *(RHI_Swapchain*)userData;
    if (!RHI_Swapchain_Resize(swapchain, width, height)) {
        printf("Swapchain resize failed\n");
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
        .title = "App",
        .width = 1280,
        .height = 720,
        .resizable = true
    });
    if (!window) {
        printf("Failed to create window\n");
        return 1;
    }

    RHI_Instance instance = RHI_Instance_Create(RHI_BACKEND_AUTO, true);
    if (!RHI_Instance_IsValid(instance)) {
        printf("Failed to create RHI instance\n");
        goto cleanup_window;
    }

    RHI_Device device = RHI_Device_Create(instance);
    if (!RHI_Device_IsValid(device)) {
        printf("Failed to create RHI device\n");
        goto cleanup_instance;
    }

    RHI_Swapchain swapchain = RHI_Swapchain_Create(instance, device, window);
    if (!RHI_Swapchain_IsValid(swapchain)) {
        printf("Failed to create RHI swapchain\n");
        goto cleanup_device;
    }

    const Vertex vertices[] = {
        {{ 0.0f,  0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
    };
    const uint16_t indices[] = {0, 1, 2};
    
    RHI_Buffer vertexBuffer = RHI_Buffer_Create(instance, device, &(RHI_BufferDesc){
        .size = sizeof(vertices), .usage = RHI_BUFFER_VERTEX | RHI_BUFFER_TRANSFER_DST,
    });
    RHI_Buffer indexBuffer = RHI_Buffer_Create(instance, device, &(RHI_BufferDesc){
        .size = sizeof(indices), .usage = RHI_BUFFER_INDEX | RHI_BUFFER_TRANSFER_DST,
    });

    if (!RHI_Buffer_IsValid(vertexBuffer) || !RHI_Buffer_IsValid(indexBuffer)) {
        printf("Failed to create triangle buffers\n");
        if (RHI_Buffer_IsValid(vertexBuffer)) RHI_Buffer_Destroy(vertexBuffer);
        if (RHI_Buffer_IsValid(indexBuffer)) RHI_Buffer_Destroy(indexBuffer);
        goto cleanup_swapchain;
    }

    RHI_Buffer vertexStaging = RHI_Buffer_Create(instance, device, &(RHI_BufferDesc){
        .size = sizeof(vertices), .usage = RHI_BUFFER_TRANSFER_SRC,
    });
    RHI_Buffer indexStaging = RHI_Buffer_Create(instance, device, &(RHI_BufferDesc){
        .size = sizeof(indices), .usage = RHI_BUFFER_TRANSFER_SRC,
    });

    bool stagingOk = RHI_Buffer_IsValid(vertexStaging) && RHI_Buffer_IsValid(indexStaging);
    stagingOk = stagingOk && RHI_Buffer_Upload(instance, device, vertexStaging, 0, sizeof(vertices), vertices);
    stagingOk = stagingOk && RHI_Buffer_Upload(instance, device, indexStaging, 0, sizeof(indices), indices);
    stagingOk = stagingOk && RHI_Buffer_Copy(instance, device, vertexBuffer, 0, vertexStaging, 0, sizeof(vertices));
    stagingOk = stagingOk && RHI_Buffer_Copy(instance, device, indexBuffer, 0, indexStaging, 0, sizeof(indices));

    if (RHI_Buffer_IsValid(vertexStaging)) RHI_Buffer_Destroy(vertexStaging);
    if (RHI_Buffer_IsValid(indexStaging)) RHI_Buffer_Destroy(indexStaging);

    if (!stagingOk) {
        printf("Failed to upload triangle buffers\n");
        RHI_Buffer_Destroy(indexBuffer);
        RHI_Buffer_Destroy(vertexBuffer);
        goto cleanup_swapchain;
    }

    RHI_PipelineLayout triangleLayout = RHI_PipelineLayout_Create(instance, device, &(RHI_PipelineLayoutDesc){});
    if (!RHI_PipelineLayout_IsValid(triangleLayout)) {
        printf("Failed to create pipeline layout\n");
        goto cleanup_buffers;
    }

    void* vsBlob = NULL; size_t vsSize = 0;
    void* psBlob = NULL; size_t psSize = 0;
    if (!ReadWholeFile("shaders/triangle_vertexMain.dxil", &vsBlob, &vsSize) ||
        !ReadWholeFile("shaders/triangle_fragmentMain.dxil", &psBlob, &psSize)) {
        printf("Failed to read compiled triangle shaders (did you run build_shaders.py?)\n");
        free(vsBlob);
        free(psBlob);
        goto cleanup_layout;
    }

    RHI_Image target = RHI_Swapchain_Get_Current_Image(swapchain);
    RHI_ImageInfo targetInfo = RHI_Image_Get_Info(target);

    RHI_Pipeline trianglePipeline = RHI_Pipeline_Create(instance, device, &(RHI_PipelineDesc){
        .layout = triangleLayout,
        .stages = (RHI_ShaderStageDesc[]){
            {RHI_SHADER_STAGE_VERTEX, vsBlob, vsSize},
            {RHI_SHADER_STAGE_FRAGMENT, psBlob, psSize},
        },
        .stageCount = 2,
        .topology = RHI_TOPOLOGY_TRIANGLE_LIST,
        .cullMode = RHI_CULL_BACK,
        .frontFace = RHI_FRONT_FACE_CLOCKWISE,
        .colorAttachmentCount = 1,
        .colorFormats = {targetInfo.format},
        .sampleCount = 1,
        .vertexAttributes = (RHI_VertexAttribute[]){
            {"POSITION", 0, 0, offsetof(Vertex, position), RHI_VERTEX_FORMAT_FLOAT2, false},
            {"COLOR", 0, 0, offsetof(Vertex, color), RHI_VERTEX_FORMAT_FLOAT3, false},
        },
        .vertexAttributeCount = 2,
    });

    free(vsBlob);
    free(psBlob);

    if (!RHI_Pipeline_IsValid(trianglePipeline)) {
        printf("Failed to create triangle pipeline\n");
        goto cleanup_layout;
    }

    Window_Set_Resize_Callback(window, OnWindowResize, &swapchain);

    printf("Success!\n");

    while (!Window_Should_Close(window)) {
        Window_Poll_Events();
        if (Window_Should_Close(window)) break;

        uint32_t frameIndex = RHI_Swapchain_Acquire_Next_Image(swapchain);
        if (RHI_Device_Begin_Frame(device, frameIndex)) {
            target = RHI_Swapchain_Get_Current_Image(swapchain);
            targetInfo = RHI_Image_Get_Info(target);
            RHI_Device_Begin_Render_Pass(device, &(RHI_RenderPass){
                .colorTarget = target,
                .clearColor = true,
                .colorClear = {0.1f, 0.2f, 0.4f, 1.0f},
                .depthClear = 1.0f,
                .viewport = {0, 0, (float)targetInfo.width, (float)targetInfo.height, 0, 1},
                .scissor = {0, 0, targetInfo.width, targetInfo.height},
            });
            RHI_Device_Bind_Pipeline(device, trianglePipeline);
            RHI_Device_Bind_Vertex_Buffer(device, 0, vertexBuffer, sizeof(Vertex));
            RHI_Device_Bind_Index_Buffer(device, indexBuffer, false);
            RHI_Device_Draw_Indexed(device, 3, 1, 0, 0, 0);
            RHI_Device_End_Render_Pass(device);
            RHI_Device_End_Frame(device);
        }
        RHI_Swapchain_Present(swapchain);
    }

    RHI_Pipeline_Destroy(trianglePipeline);
cleanup_layout:
    RHI_PipelineLayout_Destroy(triangleLayout);
cleanup_buffers:
    RHI_Buffer_Destroy(indexBuffer);
    RHI_Buffer_Destroy(vertexBuffer);
cleanup_swapchain:
    RHI_Swapchain_Destroy(swapchain);
cleanup_device:
    RHI_Device_Destroy(device);
cleanup_instance:
    RHI_Instance_Destroy(instance);
cleanup_window:
    Window_Destroy(window);

    return 0;
}