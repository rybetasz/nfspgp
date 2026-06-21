#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include "Patches.h"
#include "Console.h"
#include "DALCareer.h"
#include "imgui-1.92.5/imgui.h"
#include "imgui-1.92.5/backends/imgui_impl_dx9.h"
#include "imgui-1.92.5/backends/imgui_impl_win32.h"
#include "C:\Program Files (x86)\directxsdk\Include\d3dx9.h"
#pragma comment(lib, "C:/Program Files (x86)/directxsdk/Lib/x86/d3dx9.lib")
#include <vector>
LPDIRECT3DDEVICE9 g_pd3dDevice = nullptr;
extern bool show_menu;
extern void RenderNotification();
extern void AchievementUpdate();
extern void LoadAchievementsSecure();
extern void DrawCarShopMenu(LPDIRECT3DDEVICE9 device);
extern bool g_ShowCarShop;

extern std::vector<Achievement> myAchievements;
//char const* (*GetLocalizedString)(DWORD StringHash) = (char const* (*)(DWORD))0x578830;
using GetNumCars_t = int(__thiscall*)(void* carDB, uint32_t collectionKey);
static GetNumCars_t Real_GetNumCars = (GetNumCars_t)NFSC::ADDR_GetNumCars;
static int __fastcall Hook_GetNumCars(void* thisCarDB, void* /*edx*/, uint32_t collectionKey)
{
    if (collectionKey == NFSC::CAREER_COLLECTION_KEY)
        NFSC::AchievementCarManager::CaptureCarDB(thisCarDB, collectionKey);

    return Real_GetNumCars(thisCarDB, collectionKey);
}
static bool InstallGetNumCarsDetour()
{
    DetourRestoreAfterWith();

    if (DetourTransactionBegin() != NO_ERROR) return false;
    if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR) return false;
    if (DetourAttach((PVOID*)&Real_GetNumCars, Hook_GetNumCars) != NO_ERROR)
    {
        DetourTransactionAbort();
        return false;
    }

    if (DetourTransactionCommit() != NO_ERROR)
        return false;

    return true;
}
int lastMenu = 0;



static void RemoveGetNumCarsDetour()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach((PVOID*)&Real_GetNumCars, Hook_GetNumCars);
    DetourTransactionCommit();
}
typedef HRESULT(STDMETHODCALLTYPE* tEndScene)(LPDIRECT3DDEVICE9 pDevice);
tEndScene oEndScene = nullptr;
WNDPROC oWndProc = nullptr;
bool imgui_initialized = false;

extern void RenderMenu(LPDIRECT3DDEVICE9 pDevice);
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (show_menu || g_ShowCarShop) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        switch (msg) {
        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
        case WM_MOUSEMOVE:
            return true;
        }
    }
    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}

HRESULT STDMETHODCALLTYPE hkEndScene(LPDIRECT3DDEVICE9 pDevice) {
    g_pd3dDevice = pDevice;

    if (!imgui_initialized) {
        D3DDEVICE_CREATION_PARAMETERS params;
        pDevice->GetCreationParameters(&params);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        static const ImWchar turkish_ranges[] = {
            0x0020, 0x00FF, // Temel Latin (A-Z, a-z, vb.)
            0x011E, 0x011F, // Ğ, ğ
            0x0130, 0x0131, // İ, ı
            0x015E, 0x015F, // Ş, ş
            0,
        };
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\tahoma.ttf", 16.0f, NULL, turkish_ranges);

        ImGui_ImplWin32_Init(params.hFocusWindow);
        ImGui_ImplDX9_Init(pDevice);
        SetupImGuiStyle();
        oWndProc = (WNDPROC)SetWindowLongPtr(params.hFocusWindow, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
        LoadAchievementsSecure();

        for (auto& ach : myAchievements) {
            if (ach.texture == nullptr) {
                D3DXCreateTextureFromFileA(pDevice, ach.iconPath, &ach.texture);
            }
        }

        
        ImGui_ImplDX9_CreateDeviceObjects();

        imgui_initialized = true;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    AchievementUpdate();
    RenderMenu(pDevice);
    RenderNotification();
    DrawCarShopMenu(pDevice);
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return oEndScene(pDevice);
}
void HookDX9() {
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) return;

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = GetForegroundWindow();

    IDirect3DDevice9* pDummyDevice = nullptr;
    if (FAILED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDummyDevice))) {
        pD3D->Release();
        return;
    }

    void** vTable = *(void***)pDummyDevice;
    oEndScene = (tEndScene)vTable[42];

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)oEndScene, hkEndScene); // EndScene'i Detours ile bağla
    DetourTransactionCommit();

    pDummyDevice->Release();
    pD3D->Release();
}

DWORD WINAPI WorkerThread(LPVOID lpParam) {
    Sleep(5000); // Oyunun yüklenmesini bekle
    LoadConfig(); // Config dosyalarını oku
    InstallGetNumCarsDetour();

    while (true) {
        RunPointerWatcher();
        NFSC::AchievementCarManager::Tick();// Değişimleri izle
        Sleep(100);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThread, NULL, 0, NULL);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)HookDX9, NULL, 0, NULL);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        RemoveGetNumCarsDetour();
    }
    return TRUE;
}