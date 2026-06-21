#include <windows.h>
#include <cmath>
#include <cstdio>
#include <iostream>
#include "Parts.h"

// --- Global Veriler ---
float CurrentWheelAngularVelocities[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
BYTE OriginalBytes[5];

// --- Oyun Fonksiyon Adresleri ---
#define ADDR_GETMODELNAMEHASH 0x007CDCA0
#define ADDR_STRINGHASH       0x00471050
#define ADDR_GETPARTNAME      0x007CD9F0
#define ADDR_PARTMESHEXISTS   0x007B0F30

typedef unsigned int(__cdecl* tStringHash)(const char* text);
tStringHash Game_StringHash = (tStringHash)ADDR_STRINGHASH;

typedef const char* (__cdecl* tGetPartName)(unsigned int hash);
tGetPartName Game_GetPartNameFromHash = (tGetPartName)ADDR_GETPARTNAME;

typedef bool(__cdecl* tPartMeshExists)(unsigned int hash);
tPartMeshExists Game_PartMeshExists = (tPartMeshExists)ADDR_PARTMESHEXISTS;

// --- 1. Hýz Takip Mekanizmasý ---
void __stdcall RecordWheelSpeed(int wheelIndex, float angularVelocity) {
    if (wheelIndex >= 0 && wheelIndex < 4) {
        CurrentWheelAngularVelocities[wheelIndex] = std::abs(angularVelocity);

        static int debugCounter = 0;
        if (debugCounter++ % 100 == 0) {
            printf("[PHYSICS] Wheel %d Speed: %.2f\n", wheelIndex, angularVelocity);
        }
    }
}

// UpdateTires Cave: 007C14BB adresindeki fld komutunu yakalar
void __declspec(naked) UpdateTiresCave() {
    static constexpr DWORD ReturnAddr = 0x007C14C1;
    __asm {
        fld dword ptr[edx + 0x80] // Orijinal tekerlek hýzý yükleme komutu
        pushad
        push dword ptr[edx + 0x80] // Parametre: angularVelocity
        push edi                    // Parametre: wheelIndex (edi döngü sayacý)
        call RecordWheelSpeed
        popad
        jmp ReturnAddr
    }
}

// --- 2. Model Deðiþtirme (Sonsuz Döngü Engellenmiþ) ---
int __fastcall HookedGetModelNameHash(void* _this, int edx, int wheelPos, int a3, int a4) {
    int modelHash;
    DWORD oldProtect;

    // Hook'u geçici olarak kaldýrýp orijinali çaðýr (Sonsuz döngüyü önler)
    VirtualProtect((void*)ADDR_GETMODELNAMEHASH, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)ADDR_GETMODELNAMEHASH, OriginalBytes, 5);

    typedef int(__fastcall* tFunc)(void*, int, int, int, int);
    modelHash = ((tFunc)ADDR_GETMODELNAMEHASH)(_this, edx, wheelPos, a3, a4);

    // Hook'u geri yaz
    BYTE jmp[5] = { 0xE9, 0, 0, 0, 0 };
    *(DWORD*)(jmp + 1) = ((DWORD)HookedGetModelNameHash - ADDR_GETMODELNAMEHASH) - 5;
    memcpy((void*)ADDR_GETMODELNAMEHASH, jmp, 5);
    VirtualProtect((void*)ADDR_GETMODELNAMEHASH, 5, oldProtect, &oldProtect);

    // --- ID Tespiti ve Model Swapping ---
    int partId = *(int*)((char*)_this + 0xC); // DBCarPart nesnesinden PartID'yi al

    if (wheelPos >= 0 && wheelPos < 4) {
        float speed = CurrentWheelAngularVelocities[wheelPos];

        // Jant ID'sini doðrulamak için (Eðer deðiþmiyorsa burayý aktif et):
        // printf("[DEBUG] Pos: %d | ID: %d | Speed: %.2f\n", wheelPos, partId, speed);

        if (speed > 3.0f) { // Belirlediðin düþük hýz eþiði
            // Jant ID'leri: 18 (FrontWheels), 33 (Attachment0)
            if (partId == 18 || partId == 33 || partId == (int)DBPart::FrontWheels || partId == (int)DBPart::Attachment0) {
                const char* originalName = Game_GetPartNameFromHash(modelHash);
                if (originalName) {
                    char blurName[128];
                    sprintf_s(blurName, "%s_BLUR", originalName);
                    unsigned int blurHash = Game_StringHash(blurName);

                    if (Game_PartMeshExists(blurHash)) {
                        printf(">>> BLUR AKTIF: %s -> %s\n", originalName, blurName);
                        return (int)blurHash;
                    }
                    else {
                        printf("!!! MODEL BULUNAMADI: %s\n", blurName);
                    }
                }
            }
        }
    }
    return modelHash;
}

// --- 3. Kurulum ve Hook Yardýmý ---
void PlaceJMP(BYTE* address, DWORD jumpTo, DWORD length) {
    DWORD oldProtect;
    VirtualProtect(address, length, PAGE_EXECUTE_READWRITE, &oldProtect);
    if (address == (BYTE*)ADDR_GETMODELNAMEHASH) memcpy(OriginalBytes, address, 5);
    address[0] = 0xE9;
    *(DWORD*)(address + 1) = (jumpTo - (DWORD)address) - 5;
    for (DWORD i = 5; i < length; i++) address[i] = 0x90;
    VirtualProtect(address, length, oldProtect, &oldProtect);
}

// Bunu dllmain.cpp içinde çaðýracaksýn
void InitRimBlur() {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);

    // UpdateTires'dan hýz verisini çekmek için
    PlaceJMP((BYTE*)0x007C14BB, (DWORD)UpdateTiresCave, 6);
    // Model seçimini hýza göre deðiþtirmek için
    PlaceJMP((BYTE*)ADDR_GETMODELNAMEHASH, (DWORD)HookedGetModelNameHash, 5);

    printf("NFS RimBlur Sistemi Yuklendi.\n");
}