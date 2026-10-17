#include "dx12.h"
#include "rhi/swapchain_internal.h"
#include "rhi/device_internal.h"
#include "rhi/image_internal.h"
#include "window/native_handle.h"
#include <stdlib.h>

static bool DX12_Swapchain_CreateFrameSync(DX12Swapchain* sc) {
    DX12Device* dxDevice = sc->device;

    HRESULT hr = dxDevice->device->lpVtbl->CreateFence(
        dxDevice->device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&sc->frameFence);
    if (FAILED(hr)) {
        return false;
    }

    sc->frameFenceEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
    if (!sc->frameFenceEvent) {
        return false;
    }

    sc->nextFenceValue = 1; 
    for (UINT i = 0; i < DX12_FRAME_COUNT; ++i) {
        sc->backbufferFenceValues[i] = 0;
    }

    return true;
}

static void DX12_Swapchain_DestroyFrameSync(DX12Swapchain* sc) {
    if (sc->frameFenceEvent) {
        CloseHandle(sc->frameFenceEvent);
        sc->frameFenceEvent = NULL;
    }
    if (sc->frameFence) {
        sc->frameFence->lpVtbl->Release(sc->frameFence);
        sc->frameFence = NULL;
    }
}

static void DX12_Swapchain_WaitForFenceValue(DX12Swapchain* sc, UINT64 value) {
    if (value == 0) {
        return; 
    }
    if (sc->frameFence->lpVtbl->GetCompletedValue(sc->frameFence) < value) {
        sc->frameFence->lpVtbl->SetEventOnCompletion(sc->frameFence, value, sc->frameFenceEvent);
        WaitForSingleObject(sc->frameFenceEvent, INFINITE);
    }
}

static bool DX12_Swapchain_CreateRTVs(DX12Swapchain* sc) {
    DX12Device* dxDevice = sc->device;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {0};
    heapDesc.NumDescriptors = DX12_FRAME_COUNT;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = dxDevice->device->lpVtbl->CreateDescriptorHeap(
        dxDevice->device, &heapDesc, &IID_ID3D12DescriptorHeap, (void**)&sc->rtvHeap);
    if (FAILED(hr)) {
        return false;
    }

    sc->rtvDescriptorSize = dxDevice->device->lpVtbl->GetDescriptorHandleIncrementSize(
        dxDevice->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE heapStart;
    sc->rtvHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(sc->rtvHeap, &heapStart);

    for (UINT i = 0; i < DX12_FRAME_COUNT; ++i) {
        hr = sc->swapchain->lpVtbl->GetBuffer(sc->swapchain, i, &IID_ID3D12Resource,
                                              (void**)&sc->backbuffers[i]);
        if (FAILED(hr)) {
            return false;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE handle = heapStart;
        handle.ptr += (SIZE_T)i * sc->rtvDescriptorSize;

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {0};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        dxDevice->device->lpVtbl->CreateRenderTargetView(
            dxDevice->device, sc->backbuffers[i], &rtvDesc, handle);

        sc->rtvHandles[i] = handle;
        sc->images[i].base.info = (RHI_ImageInfo){sc->width, sc->height, RHI_IMAGE_FORMAT_RGBA8_SRGB};
        sc->images[i].owner = sc;
        sc->images[i].index = i;
        sc->images[i].resource = sc->backbuffers[i];
        sc->imageHandles[i] = RHI_Image_Register(&sc->images[i].base);
    }

    return true;
}

static void DX12_Swapchain_ReleaseRTVs(DX12Swapchain* sc) {
    for (UINT i = 0; i < DX12_FRAME_COUNT; ++i) {
        if (RHI_Image_IsValid(sc->imageHandles[i])) {
            RHI_Image_Unregister(sc->imageHandles[i]);
            sc->imageHandles[i] = RHI_Image_Invalid();
        }
        if (sc->backbuffers[i]) {
            sc->backbuffers[i]->lpVtbl->Release(sc->backbuffers[i]);
            sc->backbuffers[i] = NULL;
        }
    }
    if (sc->rtvHeap) {
        sc->rtvHeap->lpVtbl->Release(sc->rtvHeap);
        sc->rtvHeap = NULL;
    }
}

static void DX12_Swapchain_Destroy(RHI_Swapchain_Impl* self) {
    DX12Swapchain* sc = (DX12Swapchain*)self;

    UINT64 lastSubmittedFence = 0;
    for (UINT i = 0; i < DX12_FRAME_COUNT; ++i)
        if (sc->backbufferFenceValues[i] > lastSubmittedFence)
            lastSubmittedFence = sc->backbufferFenceValues[i];
    DX12_Swapchain_WaitForFenceValue(sc, lastSubmittedFence);

    DX12_Swapchain_DestroyFrameSync(sc);
    DX12_Swapchain_ReleaseRTVs(sc);
    if (sc->swapchain) {
        sc->swapchain->lpVtbl->Release(sc->swapchain);
    }
    free(sc);
}

static bool DX12_Swapchain_Resize(RHI_Swapchain_Impl* self, uint32_t width, uint32_t height) {
    DX12Swapchain* sc = (DX12Swapchain*)self;

    for (UINT i = 0; i < DX12_FRAME_COUNT; ++i) {
        DX12_Swapchain_WaitForFenceValue(sc, sc->backbufferFenceValues[i]);
        sc->backbufferFenceValues[i] = 0;
    }

    DX12_Swapchain_ReleaseRTVs(sc);

    HRESULT hr = sc->swapchain->lpVtbl->ResizeBuffers(
        sc->swapchain, DX12_FRAME_COUNT, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        return false;
    }

    sc->width = width;
    sc->height = height;
    if (!DX12_Swapchain_CreateRTVs(sc)) {
        return false;
    }
    sc->currentBackbufferIndex = sc->swapchain->lpVtbl->GetCurrentBackBufferIndex(sc->swapchain);
    return true;
}

static uint32_t DX12_Swapchain_AcquireNextImage(RHI_Swapchain_Impl* self) {
    DX12Swapchain* sc = (DX12Swapchain*)self;

    sc->currentBackbufferIndex = sc->swapchain->lpVtbl->GetCurrentBackBufferIndex(sc->swapchain);

    DX12_Swapchain_WaitForFenceValue(sc, sc->backbufferFenceValues[sc->currentBackbufferIndex]);

    return sc->currentBackbufferIndex;
}

static RHI_Image DX12_Swapchain_GetCurrentImage(RHI_Swapchain_Impl* self) {
    DX12Swapchain* sc = (DX12Swapchain*)self;
    return sc->imageHandles[sc->currentBackbufferIndex];
}

static void DX12_Swapchain_PresentImpl(RHI_Swapchain_Impl* self) {
    DX12Swapchain* sc = (DX12Swapchain*)self;

    sc->swapchain->lpVtbl->Present(sc->swapchain, 1 /* vsync */, 0);

    UINT64 value = sc->nextFenceValue++;
    sc->device->graphicsQueue->lpVtbl->Signal(sc->device->graphicsQueue, sc->frameFence, value);
    sc->backbufferFenceValues[sc->currentBackbufferIndex] = value;
}

static const RHI_Swapchain_Vtable dx12SwapchainVtable = {
    .destroy = DX12_Swapchain_Destroy,
    .resize = DX12_Swapchain_Resize,
    .acquireNextImage = DX12_Swapchain_AcquireNextImage,
    .getCurrentImage = DX12_Swapchain_GetCurrentImage,
    .present = DX12_Swapchain_PresentImpl,
};

bool DX12_Swapchain_Create(RHI_Device device, Window* window, RHI_Swapchain_Impl** swapchain) {
    NativeWindowHandle nativeHandle = Window_Get_Native_Handle(window);
    if (nativeHandle.platform != PLATFORM_WIN32) {
        return false;
    }

    uint32_t width, height;
    Window_Get_Size(window, &width, &height);

    RHI_Device_Impl** device_impl = RHI_Device_Get_Impl(device);
    if (!device_impl || !*device_impl) {
        return false;
    }
    DX12Device* dxDevice = (DX12Device*)*device_impl;

    DX12Swapchain* sc = calloc(1, sizeof(DX12Swapchain));
    if (!sc) {
        return false;
    }

    sc->base.vtable = &dx12SwapchainVtable;
    sc->device = dxDevice;
    sc->hwnd = (HWND)nativeHandle.win32.hwnd;
    sc->width = width;
    sc->height = height;

    DXGI_SWAP_CHAIN_DESC1 desc = {0};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = DX12_FRAME_COUNT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    IDXGISwapChain1* swapchain1 = NULL;
    HRESULT hr = dxDevice->instance->factory->lpVtbl->CreateSwapChainForHwnd(
        dxDevice->instance->factory,
        (IUnknown*)dxDevice->graphicsQueue,
        sc->hwnd, &desc, NULL, NULL, &swapchain1);

    if (FAILED(hr)) {
        free(sc);
        return false;
    }

    dxDevice->instance->factory->lpVtbl->MakeWindowAssociation(
        dxDevice->instance->factory, sc->hwnd, DXGI_MWA_NO_ALT_ENTER);

    hr = swapchain1->lpVtbl->QueryInterface(swapchain1, &IID_IDXGISwapChain4, (void**)&sc->swapchain);
    swapchain1->lpVtbl->Release(swapchain1);

    if (FAILED(hr)) {
        free(sc);
        return false;
    }

    if (!DX12_Swapchain_CreateRTVs(sc)) {
        sc->swapchain->lpVtbl->Release(sc->swapchain);
        free(sc);
        return false;
    }

    if (!DX12_Swapchain_CreateFrameSync(sc)) {
        DX12_Swapchain_ReleaseRTVs(sc);
        sc->swapchain->lpVtbl->Release(sc->swapchain);
        free(sc);
        return false;
    }

    sc->currentBackbufferIndex = sc->swapchain->lpVtbl->GetCurrentBackBufferIndex(sc->swapchain);

    *swapchain = &sc->base;
    return true;
}
