#include <windows.h>
#include <vector>
#include <string>
#include <map>
#include "Patches.h"

struct PointerSwap {
    unsigned int carHash;
    unsigned int targetHash;
    unsigned int replacementHash;
};

std::vector<PointerSwap> g_Swaps;
std::map<unsigned int, uintptr_t> g_OriginalBackups;


#define LANGSTUFF1_PTR 0x00A95378
#define LANGSTUFF2_COUNT 0x00A95384

void RunPointerWatcher() {
    // Aktif Araç Kontrolü
    unsigned int* pActiveCar = (unsigned int*)0x00B74530;
    if (IsBadReadPtr(pActiveCar, 4)) return;
    unsigned int currentCar = *pActiveCar;

    for (int tableIdx = 0; tableIdx < 3; tableIdx++) {
        uintptr_t* tableArray = (uintptr_t*)LANGSTUFF1_PTR;
        uintptr_t stringTable = tableArray[tableIdx];

        int* countArray = (int*)LANGSTUFF2_COUNT;
        int elementCount = countArray[tableIdx];

        if (!stringTable || elementCount <= 0 || IsBadReadPtr((void*)stringTable, 4))
            continue;

        for (auto& item : g_Swaps) {
            // Tabloyu eleman sayýsý kadar tara
            for (int i = 0; i < elementCount; i++) {
                uintptr_t entry = stringTable + (i * 8);
                unsigned int tableHash = *(unsigned int*)entry;

                if (tableHash == item.targetHash) {
                    uintptr_t* pPointerInTable = (uintptr_t*)(entry + 4);

                    if (currentCar == item.carHash) {
                        // REPLACEMENT HASH'i bul (Ayný tabloda veya diðer tablolarda olabilir)
                        uintptr_t newAddr = 0;
                        for (int t = 0; t < 3; t++) {
                            uintptr_t st = tableArray[t];
                            int ec = countArray[t];
                            if (!st) continue;
                            for (int j = 0; j < ec; j++) {
                                if (*(unsigned int*)(st + (j * 8)) == item.replacementHash) {
                                    newAddr = *(uintptr_t*)(st + (j * 8) + 4);
                                    break;
                                }
                            }
                            if (newAddr) break;
                        }

                        if (newAddr && *pPointerInTable != newAddr) {
                            if (g_OriginalBackups.find(item.targetHash) == g_OriginalBackups.end()) {
                                g_OriginalBackups[item.targetHash] = *pPointerInTable;
                            }
                            *pPointerInTable = newAddr;
                        }
                    }
                    else {
                        // GERÝ YÜKLE
                        if (g_OriginalBackups.count(item.targetHash)) {
                            uintptr_t backup = g_OriginalBackups[item.targetHash];
                            if (*pPointerInTable != backup) {
                                *pPointerInTable = backup;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
}
// LoadConfig fonksiyonu ayný kalacak...

void LoadConfig() {
    const char* path = ".\\scripts\\CustomPerfStrings.ini";
    char sections[4096];
    if (GetPrivateProfileSectionNamesA(sections, 4096, path) <= 0) return;

    std::map<std::string, unsigned int> globalTags;
    char globalData[8192];
    GetPrivateProfileSectionA("Global", globalData, 8192, path);

    char* p = globalData;
    while (*p) {
        std::string line = p;
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            unsigned int h = (unsigned int)strtoul(line.substr(0, eq).c_str(), NULL, 16);
            globalTags[line.substr(eq + 1)] = h;
        }
        p += strlen(p) + 1;
    }

    char* s = sections;
    while (*s) {
        if (s[0] == '0' && _stricmp(s, "Global") != 0) {
            unsigned int carH = (unsigned int)strtoul(s, NULL, 16);
            char carData[8192];
            GetPrivateProfileSectionA(s, carData, 8192, path);
            char* cl = carData;
            while (*cl) {
                std::string line = cl;
                size_t eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string tagName = line.substr(0, eq);
                    unsigned int repHash = (unsigned int)strtoul(line.substr(eq + 1).c_str(), NULL, 16);
                    if (globalTags.count(tagName)) {
                        PointerSwap ps;
                        ps.carHash = carH;
                        ps.targetHash = globalTags[tagName];
                        ps.replacementHash = repHash;
                        g_Swaps.push_back(ps);
                    }
                }
                cl += strlen(cl) + 1;
            }
        }
        s += strlen(s) + 1;
    }
}