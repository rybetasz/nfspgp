#pragma once

#include <windows.h>
#include <cstdint>
#include <string>

// ============================================================
// SLOT POOL DATA STRUCTURES
// ============================================================

struct SlotData
{
    uint32_t used = 0;
    uint32_t total = 0;

    uint64_t usedMemory = 0;
    uint64_t totalMemory = 0;

    bool valid = false;
};

struct PoolData
{
    SlotData worldModel;
    SlotData carPartModel;
    SlotData ecstacyModel;
    SlotData texturePack;
    SlotData ePoly;
};

// ============================================================
// EXPORTED FUNCTIONS
// ============================================================

bool InstallSFXObjDetours();
void RemoveSFXObjDetours();

void UpdatePoolData();
void DrawPoolWatermark();