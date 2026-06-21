#include "Patches.h"
#include <stdio.h>

namespace GameUI {
    // Carbon v1.4 Kesinleþmiþ Adresler
    const uintptr_t ADDR_CREATE_DIALOG = 0x005C9F30; // CreateDialogScreen
    const uintptr_t ADDR_SHOW_OK = 0x005D67D0; // ShowOk
    const uintptr_t ADDR_SHOW_DIALOG = 0x005CC810; // ShowDialog
    const uintptr_t ADDR_INSTANCE = 0x00A7206C; // sInstance
    const uintptr_t ADDR_BUILD_MOUSE = 0x0055BC10; // BuildMouseObjectStateList
    const uintptr_t ADDR_FIND_PACKAGE = 0x0055EFD0; // FindPackage

    bool g_NotifyPending = false;
    char g_MsgBuffer[1024];

    void Notify(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vsnprintf(g_MsgBuffer, sizeof(g_MsgBuffer), format, args);
        va_end(args);
        g_NotifyPending = true;
    }

    void __stdcall Update() {
        if (!g_NotifyPending) return;

        uintptr_t* pInstance = (uintptr_t*)0x00A7206C;

        // 1. Instance yoksa yarat (Main Thread içinde)
        if (*pInstance == 0) {
            typedef void* (__cdecl* tCreate)(void*);
            ((tCreate)0x005C9F30)(NULL);
        }

        uintptr_t instance = *pInstance;
        if (instance) {
            // 2. Metni hazýrla (v1.4)
            typedef void(__cdecl* tShowOk)(const char*);
            ((tShowOk)0x005D67D0)(g_MsgBuffer);

            // 3. Menüyü Görünür Yap (ShowDialog)
            __asm {
                mov ecx, instance
                call ds : [0x005CC810]
            }

            // 4. Çizim Bayraðýný Tetikle (Zorla Render)
            // Eðer menü görünmüyorsa, oyunun UI çizicisini (FEng) dürtmemiz gerekir
            *(BYTE*)(instance + 0x24) = 1; // IsVisible bayraðý
        }

        g_NotifyPending = false;
    }

    // --- HOOK --- (0x0071E560 v1.4 için ana döngü)
    void __declspec(naked) MainLoopHook() {
        __asm {
            pushad
            call Update
            popad
            mov eax, [esi + 8] // Orijinal komut
            push 0x0071E565
            retn
        }
    }

    void ApplyHook() {
        DWORD old;
        VirtualProtect((void*)0x0071E560, 5, PAGE_EXECUTE_READWRITE, &old);
        *(BYTE*)0x0071E560 = 0xE9;
        *(DWORD*)(0x0071E560 + 1) = (uintptr_t)MainLoopHook - 0x0071E560 - 5;
        VirtualProtect((void*)0x0071E560, 5, old, &old);
    }
}

// --- KULLANIM ÖRNEÐÝ ---
void Test() {
    if (GetAsyncKeyState(0x35) & 0x8000) {
        GameUI::Notify("SPIKE STRIP AKTIF!\nv1.4 Mod Yuklendi.");
    }
}