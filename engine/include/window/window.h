#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Window Window;

typedef struct {
    const char* title;
    uint32_t    width;
    uint32_t    height;
    bool        resizable;
} WindowDesc;

Window* Window_Create(const WindowDesc* desc);
void Window_Destroy(Window* window);
bool Window_Should_Close(Window* window);
void Window_Poll_Events(void);
void Window_Get_Size(Window* window, uint32_t* outWidth, uint32_t* outHeight);

typedef void (*WindowResizeCallback)(void* userData, uint32_t width, uint32_t height);
void Window_Set_Resize_Callback(Window* window, WindowResizeCallback callback, void* userData);

typedef struct NativeWindowHandle NativeWindowHandle;
NativeWindowHandle Window_Get_Native_Handle(Window* window);

#endif