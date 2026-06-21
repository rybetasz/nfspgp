#include <windows.h>
#include "Patches.h"

namespace Cheats {
    bool GhostCarEnabled = false;
    BYTE GhostCarOrig[6] = { 0x8B, 0x86, 0x64, 0x01, 0x00, 0x00 };
}
void __declspec(naked) GhostCarCave() {
    __asm {
        mov eax, 0x00A7D30
        mov eax, [eax]
        test eax, eax
        jz _original
        mov eax, [eax]
        test eax, eax
        jz _original
        mov eax, [eax + 0x70]
        test eax, eax
        jz _original
        sub eax, 0x40
        cmp eax, esi
        je _ghost
        _original :
        mov eax, [esi + 0x0164]
            push 0x00709A89
            ret

            _ghost :
        mov eax, [esi + 0x0164]
            push 0x00709A8D
            ret
    }
}

void SetInfiniteNos(bool enable) {
    uintptr_t address = 0x6E404C;
    DWORD oldProtect;
    VirtualProtect((void*)address, 6, PAGE_EXECUTE_READWRITE, &oldProtect);

    if (enable) {
        *(BYTE*)(address + 2) = 0xF4;
    }
    else {
        *(BYTE*)(address + 2) = 0xF0;
    }

    VirtualProtect((void*)address, 6, oldProtect, &oldProtect);
}

void SetGhostCar(bool enable) {
    uintptr_t address = 0x00709A83;
    DWORD oldProtect;

    if (VirtualProtect((void*)address, 6, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        if (enable) {
            BYTE jmpCode[6] = { 0xE9, 0x00, 0x00, 0x00, 0x00, 0x90 };
            DWORD relAddr = (DWORD)GhostCarCave - (address + 5);
            memcpy(&jmpCode[1], &relAddr, 4);

            memcpy((void*)address, jmpCode, 6);
            Cheats::GhostCarEnabled = true;
        }
        else {
            // Kapatýldýðýnda orijinal baytlarý geri yükle
            memcpy((void*)address, Cheats::GhostCarOrig, 6);
            Cheats::GhostCarEnabled = false;
        }
        VirtualProtect((void*)address, 6, oldProtect, &oldProtect);
    }
}