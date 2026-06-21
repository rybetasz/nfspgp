#pragma once
#include <windows.h>
#include <cstdint>
void LoadConfig();
void RunPointerWatcher();
void SetInfiniteNos(bool enable);
void SetGhostCar(bool enable);
void SetupImGuiStyle();
void InitMapIconExtender();
void InitRaceTypeExtender();
void Patch(uintptr_t address, const void* data, size_t size);