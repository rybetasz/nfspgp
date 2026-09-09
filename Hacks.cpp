#include <windows.h>
#include "Patches.h"
#include "DALCareer.h"

void SetInfiniteNos(bool enable) {
    const uintptr_t address = 0x6E404C;
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

void SetForceCheckpointVisible(bool enable) {
  const uintptr_t address = 0x0063ED73;
    DWORD oldProtect;
    VirtualProtect((void*)address, 2, PAGE_EXECUTE_READWRITE, &oldProtect);

    if (enable) {
        *(BYTE*)(address) = 0xB0;
        *(BYTE*)(address + 1) = 0x01;
    }
    else {
        *(BYTE*)(address) = 0x8A;
        *(BYTE*)(address + 1) = 0x00;
    }

    VirtualProtect((void*)address, 2, oldProtect, &oldProtect);
}
void GameForcePursuitStart(int enable)
{
	Game::GameForcePursuitStart(enable);
}