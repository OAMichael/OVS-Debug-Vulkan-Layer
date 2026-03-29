#ifndef UTILS__VULKAN_PLATFORM_H
#define UTILS__VULKAN_PLATFORM_H

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#else

#include <cstdint>

typedef void* HINSTANCE;
typedef void* HWND;
typedef void* HANDLE;
typedef void* HMONITOR;
typedef wchar_t* LPWSTR;
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
