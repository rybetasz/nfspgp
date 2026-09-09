#include "PoolTracker.h"

#include <cstdio>
#include <sstream>
#include <iomanip>
#include <mutex>

#include "detours.h"
#include "imgui-1.92.5/imgui.h"

// ============================================================
// ADDRESSES & CONSTANTS
// ============================================================

static constexpr uintptr_t SLOTPOOL_SENTINEL = 0x00A87C6C;
static constexpr uintptr_t WORLD_MODEL_POOL = 0x00B74D1C;

static constexpr uintptr_t SFXOBJ_CREATE = 0x00556350;
static constexpr uintptr_t SFXOBJ_DESTROY = 0x005563E0;
static constexpr uint32_t  SFXOBJ_SIZE = 0x2C0;

// ============================================================
// INTERNAL GLOBALS
// ============================================================

static volatile LONG g_SFXObjCount = 0;
static PoolData g_poolData;
static std::mutex g_poolMutex;

// ============================================================
// SFXOBJ HOOK DEFINITIONS
// ============================================================

using SFXObjCreate_t = void* (__cdecl*)(int arg0);
static SFXObjCreate_t Real_SFXObjCreate = reinterpret_cast<SFXObjCreate_t>(SFXOBJ_CREATE);

static void* __cdecl Hook_SFXObjCreate(int arg0)
{
    void* result = Real_SFXObjCreate(arg0);
    if (result)
    {
        InterlockedIncrement(&g_SFXObjCount);
    }
    return result;
}

using SFXObjDestroy_t = void* (__thiscall*)(void* thisPtr, int flags);
static SFXObjDestroy_t Real_SFXObjDestroy = reinterpret_cast<SFXObjDestroy_t>(SFXOBJ_DESTROY);

static void* __fastcall Hook_SFXObjDestroy(void* thisPtr, void* /*edx*/, int flags)
{
    void* result = Real_SFXObjDestroy(thisPtr, flags);
    if (thisPtr)
    {
        LONG count = InterlockedCompareExchange(&g_SFXObjCount, 0, 0);
        if (count > 0)
        {
            InterlockedDecrement(&g_SFXObjCount);
        }
    }
    return result;
}

// ============================================================
// INSTALL / REMOVE HOOKS
// ============================================================

bool InstallSFXObjDetours()
{
    DetourRestoreAfterWith();

    if (DetourTransactionBegin() != NO_ERROR)
        return false;

    if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR)
    {
        DetourTransactionAbort();
        return false;
    }

    if (DetourAttach(reinterpret_cast<PVOID*>(&Real_SFXObjCreate), Hook_SFXObjCreate) != NO_ERROR)
    {
        DetourTransactionAbort();
        return false;
    }

    if (DetourAttach(reinterpret_cast<PVOID*>(&Real_SFXObjDestroy), Hook_SFXObjDestroy) != NO_ERROR)
    {
        DetourTransactionAbort();
        return false;
    }

    return DetourTransactionCommit() == NO_ERROR;
}

void RemoveSFXObjDetours()
{
    if (DetourTransactionBegin() != NO_ERROR)
        return;

    DetourUpdateThread(GetCurrentThread());

    DetourDetach(reinterpret_cast<PVOID*>(&Real_SFXObjCreate), Hook_SFXObjCreate);
    DetourDetach(reinterpret_cast<PVOID*>(&Real_SFXObjDestroy), Hook_SFXObjDestroy);

    DetourTransactionCommit();
}

// ============================================================
// SAFE MEMORY READ
// ============================================================

template<typename T>
static bool ReadMemory(uintptr_t address, T& value)
{
    __try
    {
        value = *reinterpret_cast<T*>(address);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        value = {};
        return false;
    }
}

// ============================================================
// SLOTPOOL RESOLUTION
// ============================================================

static uintptr_t FindSlotPool(const char* wantedName)
{
    uint32_t sentinelValue = 0;
    if (!ReadMemory(SLOTPOOL_SENTINEL, sentinelValue))
        return 0;

    uintptr_t sentinel = static_cast<uintptr_t>(sentinelValue);
    if (!sentinel)
        return 0;

    uint32_t firstValue = 0;
    if (!ReadMemory(sentinel + 0x00, firstValue))
        return 0;

    uintptr_t current = static_cast<uintptr_t>(firstValue);

    for (uint32_t i = 0; i < 10000; ++i)
    {
        if (!current || current == sentinel)
            break;

        uint32_t nameAddress = 0;
        if (!ReadMemory(current + 0x0C, nameAddress))
            break;

        const char* name = reinterpret_cast<const char*>(static_cast<uintptr_t>(nameAddress));
        if (name)
        {
            bool match = false;
            __try
            {
                match = (strcmp(name, wantedName) == 0);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                match = false;
            }

            if (match)
                return current;
        }

        uint32_t nextValue = 0;
        if (!ReadMemory(current + 0x00, nextValue))
            break;

        uintptr_t next = static_cast<uintptr_t>(nextValue);
        if (next == current)
            break;

        current = next;
    }

    return 0;
}

static SlotData ReadSlotPool(const char* name)
{
    SlotData result{};
    uintptr_t pool = FindSlotPool(name);

    if (!pool)
        return result;

    uint32_t allocCount = 0;
    uint32_t slotSize = 0;
    uint32_t totalSlots = 0;

    if (!ReadMemory(pool + 0x18, allocCount) ||
        !ReadMemory(pool + 0x28, slotSize) ||
        !ReadMemory(pool + 0x2C, totalSlots))
    {
        return result;
    }

    if (allocCount > totalSlots)
        allocCount = totalSlots;

    result.used = allocCount;
    result.total = totalSlots;
    result.usedMemory = static_cast<uint64_t>(allocCount) * static_cast<uint64_t>(slotSize);
    result.totalMemory = static_cast<uint64_t>(totalSlots) * static_cast<uint64_t>(slotSize);
    result.valid = true;

    return result;
}

// ============================================================
// DATA UPDATE & FORMAT
// ============================================================

void UpdatePoolData()
{
    PoolData data{};

    data.worldModel = ReadSlotPool("WorldModelSlotPool");
    data.carPartModel = ReadSlotPool("CarPartModelPool");
    data.ecstacyModel = ReadSlotPool("Ecstacy:ModelSlotPool");
    data.texturePack = ReadSlotPool("TexturePackSlotPool");
    data.ePoly = ReadSlotPool("ePolySlotPool");

    {
        std::lock_guard<std::mutex> lock(g_poolMutex);
        g_poolData = data;
    }
}

static std::string FormatKB(uint64_t bytes)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / 1024.0) << " KB";
    return ss.str();
}

// ============================================================
// WATERMARK RENDER
// ============================================================

void DrawPoolWatermark()
{
    PoolData data{};
    {
        std::lock_guard<std::mutex> lock(g_poolMutex);
        data = g_poolData;
    }

    LONG sfxCount = InterlockedCompareExchange(&g_SFXObjCount, 0, 0);
    if (sfxCount < 0)
        sfxCount = 0;

    uint64_t sfxMemory = static_cast<uint64_t>(sfxCount) * static_cast<uint64_t>(SFXOBJ_SIZE);

    ImGuiViewport* vp = ImGui::GetMainViewport();

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos(
        ImVec2(vp->WorkPos.x + vp->WorkSize.x - 10.0f, vp->WorkPos.y + 10.0f),
        ImGuiCond_Always,
        ImVec2(1.0f, 0.0f)
    );

    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGui::Begin("##PoolWatermark", nullptr, flags);

    ImGui::Text("POOL USAGE");
    ImGui::Separator();

    if (data.worldModel.valid)
        ImGui::Text("WorldModel       %u / %u  %s / %s", data.worldModel.used, data.worldModel.total, FormatKB(data.worldModel.usedMemory).c_str(), FormatKB(data.worldModel.totalMemory).c_str());
    else
        ImGui::Text("WorldModel       N/A");

    if (data.carPartModel.valid)
        ImGui::Text("CarPartModel     %u / %u  %s / %s", data.carPartModel.used, data.carPartModel.total, FormatKB(data.carPartModel.usedMemory).c_str(), FormatKB(data.carPartModel.totalMemory).c_str());
    else
        ImGui::Text("CarPartModel     N/A");

    if (data.ecstacyModel.valid)
        ImGui::Text("Ecstacy:Model    %u / %u  %s / %s", data.ecstacyModel.used, data.ecstacyModel.total, FormatKB(data.ecstacyModel.usedMemory).c_str(), FormatKB(data.ecstacyModel.totalMemory).c_str());
    else
        ImGui::Text("Ecstacy:Model    N/A");

    if (data.texturePack.valid)
        ImGui::Text("TexturePack      %u / %u  %s / %s", data.texturePack.used, data.texturePack.total, FormatKB(data.texturePack.usedMemory).c_str(), FormatKB(data.texturePack.totalMemory).c_str());
    else
        ImGui::Text("TexturePack      N/A");

    if (data.ePoly.valid)
        ImGui::Text("ePoly            %u / %u  %s / %s", data.ePoly.used, data.ePoly.total, FormatKB(data.ePoly.usedMemory).c_str(), FormatKB(data.ePoly.totalMemory).c_str());
    else
        ImGui::Text("ePoly            N/A");

    ImGui::Separator();
    ImGui::Text("SFXObj_PFEATrax");
    ImGui::Text("  Objects    %ld", sfxCount);
    ImGui::Text("  ObjectMem  %s", FormatKB(sfxMemory).c_str());
    ImGui::Text("  Size       %u bytes", SFXOBJ_SIZE);

    ImGui::End();
}