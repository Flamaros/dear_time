#pragma once

#include <Windows.h> // @TODO remove it

void d3d11_init();
void d3d11_shutdown();

void d3d11_present_framebuffer();
void d3d11_resize_framebuffer(UINT width, UINT height);

// @TODO move to cpp, should not be used directly in main
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
 