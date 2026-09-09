#pragma once
#include "framework.h"
#pragma comment(lib, "detours.lib")
#define FUNC(address, return_t, callconv, name, ...) return_t (callconv* name)(__VA_ARGS__) = reinterpret_cast<decltype(name)>(address)
typedef unsigned int Hash;
class DALCareer
{
public:
	inline static FUNCTION_PTR(bool, __stdcall, GetCash, 0x004A0940, int* amount);
	inline static FUNCTION_PTR(bool, __stdcall, GetTutorialComplete, 0x004A0A90, int* isComplete);
	inline static FUNCTION_PTR(bool, __stdcall, GetProfileName, 0x004A0C10, const char* name, int a2);
	inline static FUNCTION_PTR(bool, __stdcall, GetCareerPercentComplete, 0x004CD1D0, float* a1);
	inline static FUNCTION_PTR(bool, __stdcall, GetGamePercentComplete, 0x004CD2B0, float* a1);
	inline static FUNCTION_PTR(float, __cdecl, GetSpeedKmh, 0x0064B3E0, void* pSimable);
    inline static FUNCTION_PTR(bool, __cdecl, Game_IsCareerMode, 0x0064BAF0);
    inline static FUNCTION_PTR(bool, __cdecl, SetCash, 0x004A0960, int amount);

};

class EngineRacer {
public:
	inline static FUNCTION_PTR(float, __thiscall, GetNOSCapacity, 0x6F1030, float* a1);
};
#pragma once
#include <windows.h>
#include <cstdint>
#include <queue>
#include <mutex>

namespace NFSC
{
    // =====================
    //  GAME ADDRESSES
    // =====================
    static constexpr uintptr_t ADDR_FEHashUpper = 0x005EA6A0;
    static constexpr uintptr_t ADDR_FindFEPresetCar = 0x007C3050;
    static constexpr uintptr_t ADDR_AwardRivalCar = 0x004D51C0;
    static constexpr uintptr_t ADDR_GetNumCars = 0x004ADA00;

    // (opsiyonel fallback için)
    static constexpr uintptr_t ADDR_CreateNewPresetCar = 0x004D1700;
    static constexpr uintptr_t ADDR_CreateNewCareerCar = 0x004D1620;

    static constexpr uint32_t  CAREER_COLLECTION_KEY = 0x000F0002;
    static constexpr int       CAREER_MAX_CARS = 10;

    // =====================
    //  FUNCTION TYPES
    // =====================
    using FEHashUpper_t = uint32_t(__cdecl*)(const char*);
    using FindFEPresetCar_t = void* (__cdecl*)(uint32_t);
    using GetNumCars_t = int(__thiscall*)(void*, uint32_t);

    // AwardRivalCar: thiscall + (presetKey)  (return EAX = created career car ptr)
    using AwardRivalCar_t = void* (__thiscall*)(void* carDB, uint32_t presetKey);

    // opsiyonel fallback
    using CreateNewPresetCar_t = void* (__thiscall*)(void*, uint32_t);
    using CreateNewCareerCar_t = void* (__thiscall*)(void*, uint32_t);

    class AchievementCarManager
    {

    public:
        static void CaptureCarDB(void* db, uint32_t collectionKey)
        {
            if (collectionKey == CAREER_COLLECTION_KEY && db)
                mCarDB = db;
        }

        static bool HasCarDB() { return mCarDB != nullptr; }
        static void ResetCarDB() { mCarDB = nullptr; }
        static void QueueAddCarByName(const char* presetName)
        {
            if (!presetName || !*presetName)
                return;

            uint32_t key = GetHash()(presetName);
            std::lock_guard<std::mutex> lk(mMutex);
            mQueue.push(key);
        }

        static void QueueAddCarByKey(uint32_t presetKey)
        {
            std::lock_guard<std::mutex> lk(mMutex);
            mQueue.push(presetKey);
        }
        static void Tick()
        {
            if (!mCarDB)
                return;

            // sanity check
            int count = GetNumCars()(mCarDB, CAREER_COLLECTION_KEY);
            if (count < 0 || count > 50)
                return;

            while (true)
            {
                uint32_t key = 0;

                {
                    std::lock_guard<std::mutex> lk(mMutex);
                    if (mQueue.empty())
                        break;

                    key = mQueue.front();
                    mQueue.pop();
                }

                if (!FindPreset()(key)) //do nothing
                {
                    continue;
                }

                // limit kontrolü
                if (GetNumCars()(mCarDB, CAREER_COLLECTION_KEY) >= CAREER_MAX_CARS)
                    break;
                AwardRivalCar()(mCarDB, key);
            }
        }

    private:
        static FEHashUpper_t GetHash()
        {
            return reinterpret_cast<FEHashUpper_t>(ADDR_FEHashUpper);
        }

        static FindFEPresetCar_t FindPreset()
        {
            return reinterpret_cast<FindFEPresetCar_t>(ADDR_FindFEPresetCar);
        }

        static GetNumCars_t GetNumCars()
        {
            return reinterpret_cast<GetNumCars_t>(ADDR_GetNumCars);
        }

        static AwardRivalCar_t AwardRivalCar()
        {
            return reinterpret_cast<AwardRivalCar_t>(ADDR_AwardRivalCar);
        }

        // fallback
        static CreateNewPresetCar_t CreatePreset()
        {
            return reinterpret_cast<CreateNewPresetCar_t>(ADDR_CreateNewPresetCar);
        }

        static CreateNewCareerCar_t CreateCareer()
        {
            return reinterpret_cast<CreateNewCareerCar_t>(ADDR_CreateNewCareerCar);
        }

        inline static void* mCarDB = nullptr;
        inline static std::queue<uint32_t> mQueue;
        inline static std::mutex mMutex;
    };
}

class cFEng
{
public:
    static cFEng* Instance()
    {
        return *(cFEng**)0x00A97A78;
    }

    bool IsPackagePushed(const char* packageName)
    {
        return ((bool(__thiscall*)(cFEng*, const char*))0x598460)(this, packageName);
    }
};

class Game
{
public:
    inline static FUNCTION_PTR(bool, __cdecl, GameForcePursuitStart, 0x006513E0, int enable);
};
