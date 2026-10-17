#include "window/window.h"
#include "window/native_handle.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(WINDOW_BACKEND_GLFW_AVAILABLE)

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
    #define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
    #define GLFW_EXPOSE_NATIVE_X11
    #define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#include <GLFW/glfw3native.h>

struct Window {
    GLFWwindow* handle;
    WindowResizeCallback resizeCallback;
    void* resizeUserData;
};

static void GLFW_FramebufferSizeCallback(GLFWwindow* glfwWindow, int width, int height) {
    Window* window = (Window*)glfwGetWindowUserPointer(glfwWindow);
    if (window && window->resizeCallback && width > 0 && height > 0) {
        window->resizeCallback(window->resizeUserData, (uint32_t)width, (uint32_t)height);
    }
}

void Window_Set_Resize_Callback(Window* window, WindowResizeCallback callback, void* userData) {
    window->resizeCallback = callback;
    window->resizeUserData = userData;
    glfwSetFramebufferSizeCallback(window->handle, GLFW_FramebufferSizeCallback);
}

Window* Window_Create(const WindowDesc* desc) {
    if (!glfwInit()) {
        return NULL;
    }

#if defined(__linux__)
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_ANY);
#endif

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, desc->resizable ? GLFW_TRUE : GLFW_FALSE);

    GLFWwindow* glfwWindow = glfwCreateWindow(
        (int)desc->width, (int)desc->height, desc->title, NULL, NULL);

    if (!glfwWindow) {
        glfwTerminate();
        return NULL;
    }

    Window* window = malloc(sizeof(Window));
    if (!window) {
        glfwDestroyWindow(glfwWindow);
        glfwTerminate();
        return NULL;
    }
    window->handle = glfwWindow;
    window->resizeCallback = NULL;
    window->resizeUserData = NULL;
    glfwSetWindowUserPointer(window->handle, window);
    return window;
}

void Window_Destroy(Window* window) {
    if (!window) return;
    glfwDestroyWindow(window->handle);
    free(window);
    glfwTerminate();
}

bool Window_Should_Close(Window* window) {
    return glfwWindowShouldClose(window->handle);
}

void Window_Poll_Events(void) {
    glfwPollEvents();
}

void Window_Get_Size(Window* window, uint32_t* outWidth, uint32_t* outHeight) {
    int w, h;
    glfwGetWindowSize(window->handle, &w, &h);
    *outWidth = (uint32_t)w;
    *outHeight = (uint32_t)h;
}

NativeWindowHandle Window_Get_Native_Handle(Window* window) {
    NativeWindowHandle handle = {0};

#if defined(_WIN32)
    handle.platform = PLATFORM_WIN32;
    handle.win32.hwnd = glfwGetWin32Window(window->handle);
    handle.win32.hinstance = GetModuleHandle(NULL);

#elif defined(__APPLE__)
    handle.platform = PLATFORM_COCOA;
    handle.cocoa.nsWindow = glfwGetCocoaWindow(window->handle);

#elif defined(__linux__)
    int platform = glfwGetPlatform();

    if (platform == GLFW_PLATFORM_WAYLAND) {
        handle.platform = PLATFORM_WAYLAND;
        handle.wayland.display = glfwGetWaylandDisplay();
        handle.wayland.surface = glfwGetWaylandWindow(window->handle);
    } else if (platform == GLFW_PLATFORM_X11) {
        handle.platform = PLATFORM_X11;
        handle.x11.display = glfwGetX11Display();
        handle.x11.window = glfwGetX11Window(window->handle);
    } else {
        fprintf(stderr, "Window_Get_Native_Handle: unexpected GLFW platform %d\n", platform);
    }
#endif

    return handle;
}

#else

void Window_Set_Resize_Callback(Window* window, WindowResizeCallback callback, void* userData) {
    printf("Window backend not supported\n");
}

Window* Window_Create(const WindowDesc* desc) {
    printf("Window backend not supported\n");
    return NULL;
}

void Window_Destroy(Window* window) {
    printf("Window backend not supported\n");
}

bool Window_Should_Close(Window* window) {
    printf("Window backend not supported\n");
    return true;
}

void Window_Poll_Events(void) {
    printf("Window backend not supported\n");
}

void Window_Get_Size(Window* window, uint32_t* outWidth, uint32_t* outHeight) {
    printf("Window backend not supported\n");
    if (outWidth) *outWidth = 0;
    if (outHeight) *outHeight = 0;
}

NativeWindowHandle Window_Get_Native_Handle(Window* window) {
    printf("Window backend not supported\n");
    NativeWindowHandle handle = {0};
    return handle;
}

#endif // WINDOW_BACKEND_GLFW_AVAILABLE