#ifndef UTILS__VULKAN_PLATFORM_H
#define UTILS__VULKAN_PLATFORM_H

#include <cstdint>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
typedef void* HINSTANCE;
typedef void* HWND;
typedef void* HANDLE;
typedef void* HMONITOR;
typedef const wchar_t* LPCWSTR;
typedef void* SECURITY_ATTRIBUTES;
typedef uint32_t DWORD;
#endif

typedef void* Display;
typedef void* Window;
typedef void* VisualID;
typedef void* xcb_connection_t;
typedef void* xcb_window_t;
typedef void* xcb_visualid_t;
typedef void* GgpStreamDescriptor;
typedef void* GgpFrameToken;
typedef void* zx_handle_t;
typedef void* IDirectFBSurface;
typedef void* IDirectFB;
typedef void* RROutput;

#endif // UTILS__VULKAN_PLATFORM_H
