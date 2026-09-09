#pragma once
#include <d3d9.h>
#include <vector>
#include <string>
void RenderMenu(LPDIRECT3DDEVICE9 pDevice);
inline HWND window = nullptr;
inline WNDPROC originalWindowProcess = nullptr;
void RenderNotification();
void SetNotify(const char* title, const char* msg, const char* album, LPDIRECT3DTEXTURE9 tex);
#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif