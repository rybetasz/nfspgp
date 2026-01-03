#include <windows.h>

// --- YAPILAR ---
struct RaceIconInfo {
    unsigned int RaceID;
    unsigned int FEFlags;
    char* ArtNamePointer;
    unsigned int NameHash;
    unsigned int TextureHash;
    unsigned int Padding[3];
};

struct RaceTypeMap {
    const char* TypeName;
    unsigned int RaceID;
};

// --- GLOBAL DEÐÝÞKENLER VE TABLOLAR ---
static char MyCustomArtName[] = "MMICON_HIGHWAY_%d";
static const char* MyHighwayTypeName = "highway";
static RaceIconInfo NewIconTable[32];
static RaceTypeMap NewTypeTable[32];
static uintptr_t NewJumpTable[40];

// Yeni Doku Atama Fonksiyonu
void __declspec(naked) CustomIconTextureLink() {
    __asm {
        mov eax, [esp + 8]
        mov dword ptr[eax], 0x698274DC // MODE_ICON_HIGHWAY Hash
        mov al, 1
        pop ecx
        retn 8
    }
}

// Güvenli Bellek Yazma Fonksiyonu
void PatchPointer(uintptr_t address, void* value) {
    DWORD oldProtect;
    VirtualProtect((void*)address, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
    *(uintptr_t*)address = (uintptr_t)value;
    VirtualProtect((void*)address, 4, oldProtect, &oldProtect);
}

void InitRacePatches() {
    DWORD oldProtect;

    // 1. TABLOLARI HAZIRLA
    // Orijinal Ýkon Tablosu (0x00A590B0) kopyalanýyor
    memcpy(NewIconTable, (void*)0x00A590B0, 16 * sizeof(RaceIconInfo));
    NewIconTable[16].RaceID = 17;
    NewIconTable[16].FEFlags = 0x400;
    NewIconTable[16].ArtNamePointer = MyCustomArtName;
    NewIconTable[16].NameHash = 0xBCC1C554;
    NewIconTable[16].TextureHash = 0x698274DC;
    NewIconTable[17].RaceID = 0xFFFFFFFF; // Bitiþ

    // Orijinal Ýsim Tablosu (0x00A61180) kopyalanýyor
    memcpy(NewTypeTable, (void*)0x00A61180, 17 * sizeof(RaceTypeMap));
    NewTypeTable[17].TypeName = MyHighwayTypeName;
    NewTypeTable[17].RaceID = 17;
    NewTypeTable[18].RaceID = 0xFFFFFFFF;

    // 2. TÜM DAL VE WORLDMAP FONKSÝYONLARINI YAMALA
    // Bu kýsým senin paylaþtýðýn listedeki fonksiyonlarýn ortak noktalarýný yamalar.

    // DALWorldMap::GetRaceTypeModeIcon (004A92C0) & GetRaceTypeNameHash (004A9280)
    // Ýkon Tablosu Yönlendirmesi
    uintptr_t iconFuncs[] = { 0x004A92C7, 0x004A9287 };
    for (int i = 0; i < 2; i++) PatchPointer(iconFuncs[i], &NewIconTable[0]);

    // Ýkon Tablosu Sýnýr (Limit) Yönlendirmesi
    uintptr_t iconLimits[] = { 0x004A92D9, 0x004A9299 };
    for (int i = 0; i < 2; i++) PatchPointer(iconLimits[i], &NewIconTable[17]);

    // Ýkon Texture/Name Hash Sütun Yönlendirmesi
    PatchPointer(0x004A92ED + 3, &NewIconTable[0].TextureHash);
    PatchPointer(0x004A92AD + 3, &NewIconTable[0].NameHash);

    // 3. GRaceParameters::GetRaceType (006136A0) YAMASI
    // Ýsim Tablosu ve ID Tablosu Yönlendirmesi
    PatchPointer(0x006136E2, &NewTypeTable[0].TypeName);
    PatchPointer(0x00613703, &NewTypeTable[0].RaceID);

    // Döngü Sýnýrýný Geniþlet (cmp esi, 11h -> 19h)
    VirtualProtect((void*)0x006136F7, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
    *(unsigned char*)0x006136F7 = 0x13;
    VirtualProtect((void*)0x006136F7, 1, oldProtect, &oldProtect);

    // 4. GETEVENTICON (TEXTURE RENDER) YAMASI
    uintptr_t* originalJumpTable = (uintptr_t*)0x004B2E8C;
    for (int i = 0; i < 14; i++) NewJumpTable[i] = originalJumpTable[i];

    uintptr_t defaultCase = 0x004B2E86;
    for (int i = 14; i < 40; i++) NewJumpTable[i] = defaultCase;
    NewJumpTable[16] = (uintptr_t)CustomIconTextureLink;

    VirtualProtect((void*)0x004B2DFC, 0x100, PAGE_EXECUTE_READWRITE, &oldProtect);
    *(unsigned char*)0x004B2DFD = 0x25; // Switch Sýnýrý
    *(uintptr_t*)0x004B2E07 = (uintptr_t)&NewJumpTable[0];
    VirtualProtect((void*)0x004B2DFC, 0x100, oldProtect, &oldProtect);
}