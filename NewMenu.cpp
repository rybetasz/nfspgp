#include "Patches.h"

// Fonksiyon Ýþaretçileri
tFEObject_Clone    FE_Clone = (tFEObject_Clone)ADDR_FE_CLONE;
tFindObjectByHash  FE_Find = (tFindObjectByHash)ADDR_FE_FIND_BY_HASH;
tSetLanguageHash   FE_SetLang = (tSetLanguageHash)ADDR_FE_SET_LANG_HASH;
tFEStateManager_Push FE_Push = (tFEStateManager_Push)ADDR_FE_STATE_PUSH;
tAddOption         FE_AddOption = (tAddOption)ADDR_FE_ADD_OPTION;
tGetIsCarUnlocked  GetIsCarUnlocked = (tGetIsCarUnlocked)ADDR_IS_CAR_UNLOCKED;

// Bellek Yama Yardýmcýsý
void MakeJMP(unsigned int at, void* dest) {
    DWORD oldProtect;
    BYTE data[5] = { 0xE9, 0, 0, 0, 0 };
    unsigned int relativeAddr = (unsigned int)dest - at - 5;
    VirtualProtect((void*)at, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(&data[1], &relativeAddr, 4);
    memcpy((void*)at, data, 5);
    VirtualProtect((void*)at, 5, oldProtect, &oldProtect);
}

// 1. ADIM: Buton Týklamasýný Yakalamak (FNG Tetikleme)
void __fastcall hkHandleOptionSelected(void* pStateManager, void* edx, unsigned int optionHash) {
    if (optionHash == MSG_ELECTRIC_MENU) {
        // "MyElectricMenu.fng" dosyasýný belleðe yükle ve aç
        FE_Push(pStateManager, "MyElectricMenu.fng", 0);
        return;
    }

    // Deðilse orijinal kod devam etsin
    typedef void(__thiscall* tOrig)(void*, unsigned int);
    ((tOrig)ADDR_HANDLE_OPTION_SEL)(pStateManager, optionHash);
}

// 2. ADIM: Ana Menüye Butonu Enjekte Etmek
void __fastcall hkMainMenu_AddOptions(void* pMainMenu) {
    // Önce orijinal butonlarý (Career, Quick Race vb.) çizdir
    typedef void(__thiscall* tOrig)(void*);
    ((tOrig)ADDR_MAINMENU_ADDOPTIONS)(pMainMenu);

    // EXIT butonunu þablon olarak bul (Hash: 0x54F5E6)
    void* pRefBtn = FE_Find(pMainMenu, 0x54F5E6);

    if (pRefBtn) {
        void* pMyBtn = FE_Clone(pRefBtn, true);
        if (pMyBtn) {
            unsigned int langHash = 0x12345678; // Language.bin'deki "ELECTRIC SYSTEM" hash'i

            FE_SetLang(pMyBtn, langHash);

            // KRÝTÝK: Klonlanan butonun tetikleyeceði mesaj ID'sini ayarla
            // [pMyBtn + 0x64] ofseti genellikle mesaj ID'sini tutar
            *(unsigned int*)((unsigned int)pMyBtn + 0x64) = MSG_ELECTRIC_MENU;

            // Butonu scroller listesine ekle
            FE_AddOption(pMainMenu, langHash, pMyBtn);
        }
    }
}

// 3. ADIM: Araç Kilit Durumunu Sorgulama (Menü içinden çaðýrabilirsin)
bool IsEVUnlocked(unsigned int carVltHash) {
    int result = 0;
    GetIsCarUnlocked(result, carVltHash);
    return (result == 1);
}

void InitElectricSystem() {
    // Ana menü buton ekleme fonksiyonuna sýz
    MakeJMP(0x00858419, hkMainMenu_AddOptions);

    // Buton týklama olayýna sýz
    MakeJMP(ADDR_HANDLE_OPTION_SEL, hkHandleOptionSelected);
}