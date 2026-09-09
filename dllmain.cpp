#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cstdio>

#include "Patches.h"
#include "Console.h"
#include "DALCareer.h"
#include "MenuMusic.h"
#include "PoolTracker.h"
#include "WorldObjectCreator.h"

#include "imgui-1.92.5/imgui.h"
#include "imgui-1.92.5/backends/imgui_impl_dx9.h"
#include "imgui-1.92.5/backends/imgui_impl_win32.h"

#include "C:\Program Files (x86)\directxsdk\Include\d3dx9.h"
#pragma comment(lib, "C:/Program Files (x86)/directxsdk/Lib/x86/d3dx9.lib")

// ============================================================
// DIRECTX / IMGUI GLOBALS & EXTERNS
// ============================================================

LPDIRECT3DDEVICE9 g_pd3dDevice = nullptr;

extern bool show_menu;
extern void RenderNotification();
extern void RenderMenu(LPDIRECT3DDEVICE9 pDevice);

extern LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam);

// ============================================================
// GET NUM CARS DETOUR
// ============================================================

using GetNumCars_t = int(__thiscall*)(void* carDB, uint32_t collectionKey);
static GetNumCars_t Real_GetNumCars = (GetNumCars_t)NFSC::ADDR_GetNumCars;

static int __fastcall Hook_GetNumCars(void* thisCarDB, void* /*edx*/, uint32_t collectionKey)
{
    if (collectionKey == NFSC::CAREER_COLLECTION_KEY)
    {
        NFSC::AchievementCarManager::CaptureCarDB(thisCarDB, collectionKey);
    }

    return Real_GetNumCars(thisCarDB, collectionKey);
}

static bool InstallGetNumCarsDetour()
{
    DetourRestoreAfterWith();

    if (DetourTransactionBegin() != NO_ERROR)
        return false;

    if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR)
    {
        DetourTransactionAbort();
        return false;
    }

    if (DetourAttach((PVOID*)&Real_GetNumCars, Hook_GetNumCars) != NO_ERROR)
    {
        DetourTransactionAbort();
        return false;
    }

    return DetourTransactionCommit() == NO_ERROR;
}

static void RemoveGetNumCarsDetour()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach((PVOID*)&Real_GetNumCars, Hook_GetNumCars);
    DetourTransactionCommit();
}

// ============================================================
// DX9 HOOK & WNDPROC
// ============================================================

typedef HRESULT(STDMETHODCALLTYPE* tEndScene)(LPDIRECT3DDEVICE9 pDevice);
tEndScene oEndScene = nullptr;
WNDPROC oWndProc = nullptr;
bool imgui_initialized = false;

LRESULT CALLBACK hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (show_menu)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        switch (msg)
        {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MOUSEMOVE:
            return true;
        }
    }

    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}

HRESULT STDMETHODCALLTYPE hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
    g_pd3dDevice = pDevice;

    if (!imgui_initialized)
    {
        D3DDEVICE_CREATION_PARAMETERS params;
        pDevice->GetCreationParameters(&params);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        static const ImWchar turkish_ranges[] =
        {
            0x0020, 0x00FF,
            0x011E, 0x011F,
            0x0130, 0x0131,
            0x015E, 0x015F,
            0
        };

        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\tahoma.ttf", 16.0f, NULL, turkish_ranges);

        ImGui_ImplWin32_Init(params.hFocusWindow);
        ImGui_ImplDX9_Init(pDevice);
        SetupImGuiStyle();

        oWndProc = (WNDPROC)SetWindowLongPtr(params.hFocusWindow, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

        ImGui_ImplDX9_CreateDeviceObjects();
        imgui_initialized = true;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RenderMenu(pDevice);
    RenderNotification();
    DrawPoolWatermark();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return oEndScene(pDevice);
}

void HookDX9()
{
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D)
        return;

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = GetForegroundWindow();

    IDirect3DDevice9* pDummyDevice = nullptr;
    if (FAILED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDummyDevice)))
    {
        pD3D->Release();
        return;
    }

    void** vTable = *(void***)pDummyDevice;
    oEndScene = (tEndScene)vTable[42];

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)oEndScene, hkEndScene);
    DetourTransactionCommit();

    pDummyDevice->Release();
    pD3D->Release();
}

// ============================================================
// CONSOLE & WORKER THREAD
// ============================================================

void CreateDebugConsole()
{
    AllocConsole();

    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);

    SetConsoleTitleA("NFS Carbon - Pool Debug");
}

DWORD WINAPI WorkerThread(LPVOID lpParam)
{
    Sleep(1000);

    LoadConfig();
    InstallGetNumCarsDetour();
    InstallSFXObjDetours();
    InitMenuMusic();
    CreateDebugConsole();

    while (true)
    {
        RunPointerWatcher();
        UpdateMenuMusic();
        UpdatePoolData();
        UpdateWorldModelGameFlow();
        NFSC::AchievementCarManager::Tick();

        Sleep(100);
    }

    return 0;
}

// ============================================================
// DLL MAIN
// ============================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThread, NULL, 0, NULL);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)HookDX9, NULL, 0, NULL);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        RemoveSFXObjDetours();
        RemoveGetNumCarsDetour();
    }

    return TRUE;
}