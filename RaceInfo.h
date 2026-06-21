#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace RaceTools
{
    // ---- IDA ----
    static constexpr uintptr_t IMAGE_BASE = 0x00400000;

    static constexpr uintptr_t ADDR_mObj_GRaceDatabase = 0x00A9828C;
    static constexpr uintptr_t ADDR_GRaceDatabase_GetRaceFromHash = 0x0061BEF0;
    static constexpr uintptr_t ADDR_GRaceParameters_GetEventHash = 0x00613890;
    static constexpr uintptr_t ADDR_GRaceDatabase_GetScoreInfo = 0x006133D0;

    static constexpr uintptr_t ADDR_RaceStarter_StartRace_ii = 0x007BBED0;

#ifndef RACETOOLS_STATS_PATH
#define RACETOOLS_STATS_PATH "GLOBAL\\racetools_stats.bin"
#endif

#ifndef RACETOOLS_KEY0
#define RACETOOLS_KEY0 0x13579BDFu
#define RACETOOLS_KEY1 0x2468ACE0u
#define RACETOOLS_KEY2 0x0F1E2D3Cu
#define RACETOOLS_KEY3 0x4B5A6978u
#endif

    // IDA VA -> runtime VA
    static inline uintptr_t Aslr(uintptr_t idaVa)
    {
        const auto base = (uintptr_t)::GetModuleHandleA(nullptr);
        return base + (idaVa - IMAGE_BASE);
    }

    static inline bool SafeReadPtr(uintptr_t addr, uintptr_t& out)
    {
        __try { out = *(uintptr_t*)addr; return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { out = 0; return false; }
    }

    static inline bool SafeReadU8(uintptr_t addr, uint8_t& out)
    {
        __try { out = *(uint8_t*)addr; return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { out = 0; return false; }
    }

    static inline void* GetRaceDb()
    {
        uintptr_t p = 0;
        if (!SafeReadPtr(Aslr(ADDR_mObj_GRaceDatabase), p)) return nullptr;
        return (void*)p;
    }

    using tGetRaceFromHash = void* (__thiscall*)(void* db, uint32_t hash);
    using tGetEventHash = uint32_t(__thiscall*)(void* raceParams);
    using tGetScoreInfo = void* (__thiscall*)(void* db, uint32_t eventHash);
    using tRaceStart = int(__cdecl*)(uint32_t hash, int mode);

    static inline tGetRaceFromHash pGetRaceFromHash = (tGetRaceFromHash)Aslr(ADDR_GRaceDatabase_GetRaceFromHash);
    static inline tGetEventHash    pGetEventHash = (tGetEventHash)Aslr(ADDR_GRaceParameters_GetEventHash);
    static inline tGetScoreInfo    pGetScoreInfo = (tGetScoreInfo)Aslr(ADDR_GRaceDatabase_GetScoreInfo);
    static inline tRaceStart       pRaceStart = (tRaceStart)Aslr(ADDR_RaceStarter_StartRace_ii);

    // -------------------------
    // Public API
    // -------------------------
    static inline bool RaceStart(uint32_t raceHash, int mode = 5, int* outRet = nullptr)
    {
        __try
        {
            const int r = pRaceStart(raceHash, mode);
            if (outRet) *outRet = r;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            if (outRet) *outRet = -1;
            return false;
        }
    }

    static inline bool IsEventCompleted(uint32_t eventHash, uint8_t* outFlags = nullptr)
    {
        if (outFlags) *outFlags = 0;

        void* db = GetRaceDb();
        if (!db) return false;

        void* scoreInfo = nullptr;
        __try { scoreInfo = pGetScoreInfo(db, eventHash); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }

        if (!scoreInfo) return false;

        uint8_t flags = 0;
        if (!SafeReadU8((uintptr_t)scoreInfo + 4u, flags)) return false;

        if (outFlags) *outFlags = flags;
        return (flags & 1u) != 0u;
    }

    static inline bool IsRaceCompleted(uint32_t raceHash, uint8_t* outFlags = nullptr, uint32_t* outEventHash = nullptr)
    {
        if (outFlags) *outFlags = 0;
        if (outEventHash) *outEventHash = 0;

        void* db = GetRaceDb();
        if (!db) return false;

        void* raceParams = nullptr;
        __try { raceParams = pGetRaceFromHash(db, raceHash); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }

        if (!raceParams) return false;

        uint32_t eventHash = 0;
        __try { eventHash = pGetEventHash(raceParams); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }

        if (outEventHash) *outEventHash = eventHash;
        return IsEventCompleted(eventHash, outFlags);
    }

    // -------------------------
    // XTEA-CTR (no STL)
    // -------------------------
    struct XteaKey { uint32_t k[4]; };

    static inline void XteaEncryptBlock(uint32_t v[2], const XteaKey& key)
    {
        uint32_t v0 = v[0], v1 = v[1], sum = 0;
        const uint32_t delta = 0x9E3779B9u;
        for (int i = 0; i < 32; i++)
        {
            v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key.k[sum & 3]);
            sum += delta;
            v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key.k[(sum >> 11) & 3]);
        }
        v[0] = v0; v[1] = v1;
    }

    static inline void XteaCtrXor(uint8_t* data, uint32_t len, uint64_t nonce, const XteaKey& key)
    {
        uint64_t counter = 0;
        uint32_t off = 0;

        while (off < len)
        {
            uint32_t block[2];
            const uint64_t in = nonce + counter;
            block[0] = (uint32_t)(in & 0xFFFFFFFFu);
            block[1] = (uint32_t)(in >> 32);

            XteaEncryptBlock(block, key);

            uint8_t ks[8];
            std::memcpy(&ks[0], &block[0], 4);
            std::memcpy(&ks[4], &block[1], 4);

            const uint32_t n = (len - off < 8u) ? (len - off) : 8u;
            for (uint32_t i = 0; i < n; i++) data[off + i] ^= ks[i];

            off += n;
            counter++;
        }
    }

    static inline bool WriteStatEncrypted(const char* tag, uint32_t hash, bool completed, uint8_t flags = 0)
    {
        const uint32_t tagLen = (tag ? (uint32_t)std::strlen(tag) : 0u);
        const uint32_t tagLenClamped = (tagLen > 255u) ? 255u : tagLen;

        const uint32_t payloadLen = 8u + 4u + 1u + 1u + 1u + tagLenClamped;

        uint8_t* buf = (uint8_t*)::HeapAlloc(::GetProcessHeap(), 0, payloadLen);
        if (!buf) return false;

        uint8_t* p = buf;
        const uint64_t tick = (uint64_t)::GetTickCount64();

        std::memcpy(p, &tick, 8); p += 8;
        std::memcpy(p, &hash, 4); p += 4;
        *p++ = completed ? 1u : 0u;
        *p++ = flags;
        *p++ = (uint8_t)tagLenClamped;
        if (tagLenClamped) std::memcpy(p, tag, tagLenClamped);

        const XteaKey key{ { RACETOOLS_KEY0, RACETOOLS_KEY1, RACETOOLS_KEY2, RACETOOLS_KEY3 } };
        const uint64_t nonce = ((uint64_t)::GetCurrentProcessId() << 32) ^ tick;

        XteaCtrXor(buf, payloadLen, nonce, key);

        FILE* f = nullptr;
        fopen_s(&f, RACETOOLS_STATS_PATH, "ab");
        if (!f)
        {
            ::HeapFree(::GetProcessHeap(), 0, buf);
            return false;
        }

        const uint32_t magic = 0x31535452u; // 'RTS1'
        fwrite(&magic, 1, 4, f);
        fwrite(&payloadLen, 1, 4, f);
        fwrite(&nonce, 1, 8, f);
        fwrite(buf, 1, payloadLen, f);
        fclose(f);

        ::HeapFree(::GetProcessHeap(), 0, buf);
        return true;
    }
}