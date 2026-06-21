#include <windows.h>
#include "Patches.h"

// --- ADRESLER ---
#define ADDR_STRING_HASH_FUNC  0x004B122C 
#define ADDR_UNLOCK_CAR_FUNC   0x004B1300 
#define ADDR_UNLOCK_SYSTEM_PTR 0x00A71238 
#define ADDR_DLC_CHECK_FUNC    0x0049EC70 

typedef void(__thiscall* tUnlockCar)(void* _this, unsigned int carHash, bool bShowNotify);
tUnlockCar UnlockCarFunc = (tUnlockCar)ADDR_UNLOCK_CAR_FUNC;

typedef unsigned int(__cdecl* tHashFunc)(const char* input);

// Trampoline için statik bellek alaný
unsigned char TrampolineStorage[20];
tHashFunc CallOriginalSafe = nullptr;

// --- ÖN BÝLDÝRÝM ---
unsigned int __cdecl MyHookedHash(const char* input);

void ApplyCrashFix() {
    uintptr_t patchAddr = ADDR_DLC_CHECK_FUNC;
    DWORD old;
    if (VirtualProtect((void*)patchAddr, 3, PAGE_EXECUTE_READWRITE, &old)) {
        *(BYTE*)(patchAddr) = 0x33; *(BYTE*)(patchAddr + 1) = 0xC0; *(BYTE*)(patchAddr + 2) = 0xC3;
        VirtualProtect((void*)patchAddr, 3, old, &old);
    }
}

void UnlockElectricCar() {
    ApplyCrashFix();
    uintptr_t* pUnlockSysPtr = (uintptr_t*)ADDR_UNLOCK_SYSTEM_PTR;
    if (pUnlockSysPtr && *pUnlockSysPtr != 0) {
        UnlockCarFunc((void*)*pUnlockSysPtr, 0x3E8D21FB, true);
    }
}

unsigned int __cdecl MyHookedHash(const char* input) {
    if (input != nullptr) {
        if (_stricmp(input, "ELEKTRIK") == 0) {
            // Çökmeyi önlemek için thread kullanmaya devam edebiliriz
            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UnlockElectricCar, NULL, 0, NULL);
            return CallOriginalSafe("nosforeverever");
        }
    }
    return CallOriginalSafe(input);
}

void InstallCheatHook() {
    uintptr_t hookAddress = ADDR_STRING_HASH_FUNC;
    DWORD oldProtect;

    // 1. Orijinal 5 byte'ý yedekle
    memcpy(TrampolineStorage, (void*)hookAddress, 5);

    // 2. Trampoline sonuna orijinal fonksiyona dönüþ JMP'si ekle
    TrampolineStorage[5] = 0xE9;
    uintptr_t jmpFrom = (uintptr_t)&TrampolineStorage[5];
    uintptr_t jmpTo = hookAddress + 5;
    *(DWORD*)(jmpFrom + 1) = jmpTo - jmpFrom - 5;

    // --- HATA DÜZELTMESÝ: TÜR DÖNÜÞTÜRME ---
    // Diziyi önce void*'a, sonra fonksiyon göstergesine çeviriyoruz
    CallOriginalSafe = (tHashFunc)(void*)TrampolineStorage;

    // Trampoline alanýný çalýþtýrýlabilir yap
    VirtualProtect(TrampolineStorage, sizeof(TrampolineStorage), PAGE_EXECUTE_READWRITE, &oldProtect);

    // 3. Orijinal fonksiyona kancayý tak
    if (VirtualProtect((void*)hookAddress, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        *(BYTE*)hookAddress = 0xE9;
        *(DWORD*)(hookAddress + 1) = (uintptr_t)MyHookedHash - hookAddress - 5;
        VirtualProtect((void*)hookAddress, 5, oldProtect, &oldProtect);
    }
}