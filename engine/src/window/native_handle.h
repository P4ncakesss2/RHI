#ifndef NATIVE_HANDLE_H
#define NATIVE_HANDLE_H

typedef enum {
    PLATFORM_WIN32,
    PLATFORM_COCOA,
    PLATFORM_X11,
    PLATFORM_WAYLAND,
} Platform;

typedef struct NativeWindowHandle {
    Platform platform;
    union {
        struct { void* hwnd; void* hinstance; } win32;
        struct { void* nsWindow; } cocoa;
        struct { void* display; unsigned long window; } x11;
        struct { void* display; void* surface; } wayland;
    };
} NativeWindowHandle;

#endif