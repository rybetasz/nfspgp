#pragma once
#include <d3d9.h>
#include <vector>
#include <string>
void RenderMenu(LPDIRECT3DDEVICE9 pDevice);
struct Achievement {
    std::string name;
    std::string description;
    const char* iconPath;
    bool unlocked;
    LPDIRECT3DTEXTURE9 texture;
};
inline HWND window = nullptr;
inline WNDPROC originalWindowProcess = nullptr;