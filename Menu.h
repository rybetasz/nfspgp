#pragma once
#include <windows.h>

// --- NFS Carbon 1.1 Adresleri ---
#define ADDR_FE_CLONE            0x005FEB80
#define ADDR_FE_FIND_BY_HASH     0x005F3760
#define ADDR_FE_SET_LANG_HASH    0x00583A90
#define ADDR_FE_STATE_PUSH       0x00593750
#define ADDR_FE_ADD_OPTION       0x005BCA70
#define ADDR_IS_CAR_UNLOCKED     0x004B3250
#define ADDR_MAINMENU_ADDOPTIONS 0x0083C420
#define ADDR_FEHASH              0x005EA670

// --- Fonksiyon Tipleri ---
typedef void* (__thiscall* tFEObject_Clone)(void* _this, bool children);
typedef void* (__thiscall* tFindObjectByHash)(void* _pkg, unsigned int hash);
typedef void(__thiscall* tSetLanguageHash)(void* _obj, unsigned int hash);
typedef void(__thiscall* tFEStateManager_Push)(void* _this, const char* pkg, int type);
typedef void(__thiscall* tAddOption)(void* _menu, unsigned int labelHash, void* pWidget);
typedef unsigned int(__cdecl* tFEHash)(const char* str);
typedef bool(__stdcall* tGetIsCarUnlocked)(int& result, unsigned int carHash);

// --- Global Fonksiyonlar ---
void InitElectricSystem();
void __fastcall hkMainMenu_AddOptions(void* pMainMenu);