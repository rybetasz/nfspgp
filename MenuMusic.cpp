#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cctype>

#include <d3d9.h>

#include "Console.h"
#include "MenuMusic.h"
#include "detours.h"


// ============================================================
// ADDRESSES
// ============================================================

#define DISABLEINTERACTIVE_FORCEEATRAX_ADDRESS 0x0053DE66
#define DISABLEINTERACTIVE_FORCERETURN_ADDRESS 0x0053DDF8

#define DISABLEINTERACTIVEPURSUIT_ADDRESS1 0x0053DEA2
#define DISABLEINTERACTIVEPURSUIT_ADDRESS2 0x0053DEE5
#define DISABLEINTERACTIVEPURSUIT_ADDRESS3 0x0053E01B
#define DISABLEINTERACTIVEPURSUIT_ADDRESS4 0x0053DFCF
#define DISABLEINTERACTIVEPURSUIT_ADDRESS5 0x0053DF7C
#define DISABLEINTERACTIVEPURSUIT_ADDRESS6 0x0053DF38

#define DISABLEINTERACTIVECANYON_ADDRESS1 0x0053DEBA
#define DISABLEINTERACTIVECANYON_ADDRESS2 0x0053DF03
#define DISABLEINTERACTIVECANYON_ADDRESS3 0x0053E048
#define DISABLEINTERACTIVECANYON_ADDRESS4 0x0053DFFC
#define DISABLEINTERACTIVECANYON_ADDRESS5 0x0053DFA9
#define DISABLEINTERACTIVECANYON_ADDRESS6 0x0053DF56

#define DISABLEINTERACTIVECREW_ADDRESS1 0x0053DEAE
#define DISABLEINTERACTIVECREW_ADDRESS2 0x0053DEF4
#define DISABLEINTERACTIVECREW_ADDRESS3 0x0053E039
#define DISABLEINTERACTIVECREW_ADDRESS4 0x0053DFED
#define DISABLEINTERACTIVECREW_ADDRESS5 0x0053DF9A
#define DISABLEINTERACTIVECREW_ADDRESS6 0x0053DF47

#define THEGAMEFLOWMANAGER_ADDRESS          0x00A99BBC

#define STARTLICENSEDMUSIC_ADDRESS          0x00553470

#define STEALSFXOBJ_ADDRESS                 0x005556A0
#define STEALSFXOBJ_EXIT_ADDRESS            0x005556A7
#define SFXOBJ_SEH                          0x0099AD58

#define MUSIC_UPDATE_ADDRESS                0x008CB1D0

// ESI = current SFXObj
// Duration = ESI - 0xF8
#define MUSIC_DURATION_OFFSET               0xF8

#define DALEVENT_GET_IS_PURSUIT_ADDRESS     0x004B3180


// ============================================================
// EXTERNAL
// ============================================================

extern void SetNotify(
    const char* title,
    const char* msg,
    LPDIRECT3DTEXTURE9 tex
);


// ============================================================
// START LICENSED MUSIC
// ============================================================

typedef unsigned int(__thiscall* StartLicensedMusic_t)
(
    void* thisPtr,
    unsigned int PathEvent
    );

static StartLicensedMusic_t StartLicensedMusic =
(StartLicensedMusic_t)STARTLICENSEDMUSIC_ADDRESS;


// ============================================================
// CURRENT SFXOBJ
// ============================================================

static void* Current_SFXObj_PFEATrax = nullptr;

static DWORD StealSFXObj_Return =
STEALSFXOBJ_EXIT_ADDRESS;


void __declspec(naked) StealSFXObj_Hook()
{
    __asm
    {
        mov Current_SFXObj_PFEATrax, ecx

        push 0FFFFFFFFh
        push SFXOBJ_SEH

        jmp StealSFXObj_Return
    }
}


// ============================================================
// WRITE JMP
// ============================================================

static void WriteJump(
    DWORD address,
    void* destination
)
{
    DWORD oldProtect;

    VirtualProtect(
        (void*)address,
        5,
        PAGE_EXECUTE_READWRITE,
        &oldProtect
    );

    *(BYTE*)address = 0xE9;

    *(DWORD*)(address + 1) =
        (DWORD)destination - address - 5;

    VirtualProtect(
        (void*)address,
        5,
        oldProtect,
        &oldProtect
    );

    FlushInstructionCache(
        GetCurrentProcess(),
        (LPCVOID)address,
        5
    );
}


// ============================================================
// THREAD SAFETY
// ============================================================

static CRITICAL_SECTION g_MusicLock;


struct MusicLockGuard
{
    MusicLockGuard()
    {
        EnterCriticalSection(&g_MusicLock);
    }

    ~MusicLockGuard()
    {
        LeaveCriticalSection(&g_MusicLock);
    }
};


// ============================================================
// MUSIC MODE
// ============================================================

enum MusicMode
{
    MUSIC_MENU,
    MUSIC_RACE,
    MUSIC_ALL
};


// ============================================================
// PENDING MUSIC
// ============================================================

static bool g_PendingMusicStart = false;

static MusicMode g_PendingMusicMode =
MUSIC_MENU;

static int g_PendingMusicDelay = 0;


// ============================================================
// MUSIC TRACK
// ============================================================

struct MusicTrack
{
    unsigned int PathEvent;

    std::string TrackName;
    std::string Artist;
    std::string Album;

    MusicMode Mode;
};


// ============================================================
// GLOBALS
// ============================================================

static std::vector<MusicTrack> g_MusicTracks;

static bool g_MenuMusicInitialized = false;

static int g_CurrentTrack = -1;

static unsigned int g_PreviousGameFlowState =
0xFFFFFFFF;


// ============================================================
// PURSUIT STATE
// ============================================================

static bool g_ClosePursuitMusic = false;

static bool g_PursuitMusicPaused = false;

static int g_TrackBeforePursuit = -1;


// ============================================================
// SHUFFLE BAGS
// ============================================================

static std::vector<int> g_MenuShuffleBag;

static std::vector<int> g_RaceShuffleBag;

static MusicMode g_LastShuffleMode =
MUSIC_ALL;


// ============================================================
// MUSIC SFXOBJ STATE
// ============================================================

static uintptr_t g_MusicSFXObj = 0;


// ============================================================
// DURATION STATE
// ============================================================

static bool g_DurationWasActive = false;

static bool g_TrackEndTriggered = false;


// ============================================================
// RE-ENTRY GUARD
// ============================================================

static bool g_MusicChangeInProgress = false;


// ============================================================
// TRACK START VALIDATION
// ============================================================

static bool g_WaitingForTrackStart = false;

static int g_TrackStartWaitFrames = 0;

static int g_TrackStartRetryCount = 0;

static int g_TrackStartConfirmFrames = 0;

static int g_FramesSinceTrackConfirmed = 0;


static const int MAX_TRACK_START_WAIT_FRAMES = 20;

static const int MAX_TRACK_START_RETRIES = 2;

static const int TRACK_START_CONFIRM_FRAMES_REQUIRED = 3;

static const int MIN_FRAMES_BEFORE_FINISH_CHECK = 30;


// ============================================================
// FORWARD DECLARATIONS
// ============================================================

static void PlayNextMusic();

static bool PlayTrack(
    int index
);

static void ShuffleBag(
    std::vector<int>& bag
);

static std::vector<int>& GetShuffleBag(
    MusicMode mode
);

static void BuildShuffleBag(
    MusicMode mode
);

static int GetRandomTrack(
    MusicMode mode
);

static void UpdatePursuitMusicState();


// ============================================================
// DISABLE INTERACTIVE - CREW
// ============================================================

static bool InstallDisableInteractiveCrew()
{
    printf(
        "[Music] Installing DisableInteractive_Crew...\n"
    );

    WriteJump(
        DISABLEINTERACTIVECREW_ADDRESS1,
        (void*)DISABLEINTERACTIVE_FORCEEATRAX_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVECREW_ADDRESS2,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVECREW_ADDRESS3,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVECREW_ADDRESS4,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVECREW_ADDRESS5,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVECREW_ADDRESS6,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    printf(
        "[Music] DisableInteractive_Crew installed.\n"
    );

    return true;
}


// ============================================================
// DISABLE INTERACTIVE - CANYON
// ============================================================

static bool InstallDisableInteractiveCanyon()
{
    printf(
        "[Music] Installing DisableInteractive_Canyon...\n"
    );

    WriteJump(
        DISABLEINTERACTIVECANYON_ADDRESS1,
        (void*)DISABLEINTERACTIVE_FORCEEATRAX_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVECANYON_ADDRESS2,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVECANYON_ADDRESS3,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVECANYON_ADDRESS4,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVECANYON_ADDRESS5,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVECANYON_ADDRESS6,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    printf(
        "[Music] DisableInteractive_Canyon installed.\n"
    );

    return true;
}


// ============================================================
// DALEVENT - IS PURSUIT
// ============================================================

typedef bool(__thiscall* DALEventGetIsPursuit_t)(
    void* thisPtr,
    int* result,
    int arg
    );

static DALEventGetIsPursuit_t DALEventGetIsPursuit =
(DALEventGetIsPursuit_t)DALEVENT_GET_IS_PURSUIT_ADDRESS;


static bool IsPursuitActive()
{
    if (*(DWORD*)THEGAMEFLOWMANAGER_ADDRESS != 6)
        return false;

    int isPursuit = 0;

    bool result =
        DALEventGetIsPursuit(
            nullptr,
            &isPursuit,
            0xFFFFFFFF
        );

    return result && isPursuit != 0;
}


// ============================================================
// RESET DURATION STATE
// ============================================================

static void ResetTrackDurationState()
{
    g_DurationWasActive = false;

    g_TrackEndTriggered = false;

    g_WaitingForTrackStart = false;

    g_TrackStartWaitFrames = 0;

    g_TrackStartRetryCount = 0;

    g_TrackStartConfirmFrames = 0;

    g_FramesSinceTrackConfirmed = 0;
}


// ============================================================
// ENTER PURSUIT PAUSE
// ============================================================

static void EnterPursuitMusicPause()
{
    if (g_PursuitMusicPaused)
        return;

    g_PursuitMusicPaused = true;

    g_TrackBeforePursuit =
        g_CurrentTrack;

    printf(
        "[Music] >>> PURSUIT MUSIC PAUSE <<< "
        "Track=%d\n",
        g_CurrentTrack
    );

    ResetTrackDurationState();
}


// ============================================================
// EXIT PURSUIT PAUSE
// ============================================================

static void ExitPursuitMusicPause()
{
    if (!g_PursuitMusicPaused)
        return;

    g_PursuitMusicPaused = false;

    printf(
        "[Music] >>> PURSUIT MUSIC RESUME <<< "
        "Track=%d\n",
        g_TrackBeforePursuit
    );

    if (
        g_TrackBeforePursuit >= 0 &&
        g_TrackBeforePursuit <
        (int)g_MusicTracks.size() &&
        Current_SFXObj_PFEATrax
        )
    {
        PlayTrack(
            g_TrackBeforePursuit
        );
    }

    g_TrackBeforePursuit = -1;
}


// ============================================================
// UPDATE PURSUIT MUSIC STATE
// ============================================================

static void UpdatePursuitMusicState()
{
    DWORD gameFlow =
        *(DWORD*)THEGAMEFLOWMANAGER_ADDRESS;

    if (g_ClosePursuitMusic)
    {
        return;
    }

    bool pursuit = false;

    if (gameFlow == 6)
    {
        pursuit = IsPursuitActive();
    }

    static bool previousPursuit = false;

    if (pursuit != previousPursuit)
    {
        printf(
            "[Pursuit] GameFlow=%u | Pursuit=%s | "
            "ClosePursuitMusic=%s\n",
            gameFlow,
            pursuit ? "YES" : "NO",
            g_ClosePursuitMusic ? "YES" : "NO"
        );

        previousPursuit = pursuit;
    }

    if (gameFlow != 6)
    {
        if (g_PursuitMusicPaused)
        {
            g_PursuitMusicPaused = false;
            g_TrackBeforePursuit = -1;

            ResetTrackDurationState();
        }

        return;
    }

    if (pursuit)
    {
        EnterPursuitMusicPause();
        return;
    }

    if (g_PursuitMusicPaused)
    {
        ExitPursuitMusicPause();
    }
}


// ============================================================
// MUSIC DURATION CHECK
// ============================================================

static void CheckNativeMusicDuration(
    void* sfxObj
)
{
    if (!sfxObj)
        return;

    if (g_PursuitMusicPaused)
    {
        return;
    }

    uintptr_t base =
        (uintptr_t)sfxObj;

    // --------------------------------------------------------
    // SFXObj değiştiyse
    // --------------------------------------------------------

    if (g_MusicSFXObj != base)
    {
        printf(
            "[Music] SFXObj changed: %08X -> %08X\n",
            (unsigned int)g_MusicSFXObj,
            (unsigned int)base
        );

        g_MusicSFXObj = base;

        g_DurationWasActive = false;

        g_TrackEndTriggered = false;
    }

    // --------------------------------------------------------
    // GameFlow
    // --------------------------------------------------------

    unsigned int state =
        *(unsigned int*)
        THEGAMEFLOWMANAGER_ADDRESS;

    // --------------------------------------------------------
    // Sadece MENU ve RACE
    // --------------------------------------------------------

    if (
        state != 3 &&
        state != 6
        )
    {
        ResetTrackDurationState();

        return;
    }

    // --------------------------------------------------------
    // Duration address
    // --------------------------------------------------------

    uintptr_t durationAddress =
        base - MUSIC_DURATION_OFFSET;

    int duration =
        *(volatile int*)durationAddress;


    // ========================================================
    // WAITING FOR TRACK START
    // ========================================================

    if (g_WaitingForTrackStart)
    {
        if (duration > 0)
        {
            g_TrackStartConfirmFrames++;

            if (
                g_TrackStartConfirmFrames <
                TRACK_START_CONFIRM_FRAMES_REQUIRED
                )
            {
                return;
            }

            g_WaitingForTrackStart = false;

            g_TrackStartWaitFrames = 0;

            g_TrackStartRetryCount = 0;

            g_TrackStartConfirmFrames = 0;

            g_DurationWasActive = true;

            g_TrackEndTriggered = false;

            g_FramesSinceTrackConfirmed = 0;

            printf(
                "[Music] Track start confirmed. "
                "Duration=%d | Track=%d\n",
                duration,
                g_CurrentTrack
            );

            return;
        }

        g_TrackStartConfirmFrames = 0;

        g_TrackStartWaitFrames++;

        if (
            g_TrackStartWaitFrames <
            MAX_TRACK_START_WAIT_FRAMES
            )
        {
            return;
        }

        printf(
            "[Music] WARNING: Track did not start. "
            "Track=%d WaitFrames=%d Retry=%d\n",
            g_CurrentTrack,
            g_TrackStartWaitFrames,
            g_TrackStartRetryCount
        );

        // ----------------------------------------------------
        // Retry
        // ----------------------------------------------------

        if (
            g_TrackStartRetryCount <
            MAX_TRACK_START_RETRIES
            )
        {
            g_TrackStartRetryCount++;

            g_TrackStartWaitFrames = 0;

            g_TrackStartConfirmFrames = 0;

            printf(
                "[Music] Retrying current track. "
                "Retry=%d/%d Track=%d\n",
                g_TrackStartRetryCount,
                MAX_TRACK_START_RETRIES,
                g_CurrentTrack
            );

            if (
                g_CurrentTrack >= 0 &&
                g_CurrentTrack <
                (int)g_MusicTracks.size() &&
                Current_SFXObj_PFEATrax
                )
            {
                const MusicTrack& track =
                    g_MusicTracks[
                        g_CurrentTrack
                    ];

                unsigned int result =
                    StartLicensedMusic(
                        Current_SFXObj_PFEATrax,
                        track.PathEvent
                    );

                printf(
                    "[Music] Retry StartLicensedMusic: "
                    "PathEvent=%u Return=%08X\n",
                    track.PathEvent,
                    result
                );
            }

            return;
        }

        // ----------------------------------------------------
        // Gerçekten başlatılamadı
        // ----------------------------------------------------

        printf(
            "[Music] ERROR: Track failed to start "
            "after %d retries. Skipping.\n",
            MAX_TRACK_START_RETRIES
        );

        g_WaitingForTrackStart = false;

        g_DurationWasActive = false;

        g_TrackEndTriggered = true;

        g_TrackStartConfirmFrames = 0;

        if (g_MusicChangeInProgress)
            return;

        g_MusicChangeInProgress = true;

        PlayNextMusic();

        g_MusicChangeInProgress = false;

        return;
    }


    // ========================================================
    // NORMAL TRACK ACTIVE
    // ========================================================

    if (duration > 0)
    {
        g_DurationWasActive = true;

        g_TrackEndTriggered = false;

        if (
            g_FramesSinceTrackConfirmed <
            MIN_FRAMES_BEFORE_FINISH_CHECK
            )
        {
            g_FramesSinceTrackConfirmed++;
        }

        return;
    }


    // ========================================================
    // TRACK FINISHED
    // ========================================================

    if (
        duration == 0 &&
        g_DurationWasActive &&
        !g_TrackEndTriggered
        )
    {
        if (
            g_FramesSinceTrackConfirmed <
            MIN_FRAMES_BEFORE_FINISH_CHECK
            )
        {
            return;
        }

        g_TrackEndTriggered = true;

        printf(
            "[Music] Track finished. "
            "SFXObj=%08X DurationAddr=%08X\n",
            (unsigned int)base,
            (unsigned int)durationAddress
        );

        if (g_MusicChangeInProgress)
            return;

        g_MusicChangeInProgress = true;

        PlayNextMusic();

        g_DurationWasActive = false;

        g_MusicChangeInProgress = false;
    }
}


// ============================================================
// NATIVE MUSIC UPDATE HOOK
// ============================================================

typedef void(__thiscall* MusicUpdate_t)
(
    void* thisPtr
    );

static MusicUpdate_t OriginalMusicUpdate =
(MusicUpdate_t)MUSIC_UPDATE_ADDRESS;


void __fastcall MusicUpdate_Hook(
    void* ecx,
    void* /*edx*/
)
{
    OriginalMusicUpdate(ecx);

    if (!ecx)
        return;

    UpdatePursuitMusicState();

    if (g_PursuitMusicPaused)
        return;

    CheckNativeMusicDuration(ecx);
}


// ============================================================
// INSTALL MUSIC UPDATE HOOK
// ============================================================

static void InstallMusicUpdateHook()
{
    LONG result;

    DetourTransactionBegin();

    DetourUpdateThread(
        GetCurrentThread()
    );

    result =
        DetourAttach(
            &(PVOID&)OriginalMusicUpdate,
            (PVOID)MusicUpdate_Hook
        );

    if (result != NO_ERROR)
    {
        DetourTransactionAbort();

        printf(
            "[Music] Failed to attach MusicUpdate hook: %ld\n",
            result
        );

        return;
    }

    result =
        DetourTransactionCommit();

    if (result != NO_ERROR)
    {
        printf(
            "[Music] Failed to commit MusicUpdate hook: %ld\n",
            result
        );

        return;
    }

    printf(
        "[Music] 008CB1D0 hook installed.\n"
    );
}


// ============================================================
// INI PATH
// ============================================================

static std::string GetIniPath()
{
    char modulePath[MAX_PATH]{};

    GetModuleFileNameA(
        nullptr,
        modulePath,
        MAX_PATH
    );

    char* slash =
        strrchr(modulePath, '\\');

    if (slash)
        *(slash + 1) = '\0';

    return std::string(modulePath) +
        "scripts\\MenuMusic.ini";
}


// ============================================================
// STRING TRIM
// ============================================================

static std::string Trim(
    const std::string& value
)
{
    size_t start = 0;

    size_t end =
        value.length();

    while (
        start < end &&
        (
            value[start] == ' ' ||
            value[start] == '\t' ||
            value[start] == '\r' ||
            value[start] == '\n'
            )
        )
    {
        start++;
    }

    while (
        end > start &&
        (
            value[end - 1] == ' ' ||
            value[end - 1] == '\t' ||
            value[end - 1] == '\r' ||
            value[end - 1] == '\n'
            )
        )
    {
        end--;
    }

    return value.substr(
        start,
        end - start
    );
}


// ============================================================
// SPLIT
// ============================================================

static std::vector<std::string> Split(
    const std::string& str,
    char delimiter
)
{
    std::vector<std::string> result;

    std::string current;

    for (
        size_t i = 0;
        i < str.length();
        i++
        )
    {
        if (str[i] == delimiter)
        {
            result.push_back(
                Trim(current)
            );

            current.clear();
        }
        else
        {
            current += str[i];
        }
    }

    result.push_back(
        Trim(current)
    );

    return result;
}


// ============================================================
// MODE PARSER
// ============================================================

static bool ParseMusicMode(
    const std::string& value,
    MusicMode& mode
)
{
    std::string modeName =
        Trim(value);

    if (
        modeName.size() >= 3 &&
        (unsigned char)modeName[0] == 0xEF &&
        (unsigned char)modeName[1] == 0xBB &&
        (unsigned char)modeName[2] == 0xBF
        )
    {
        modeName.erase(
            0,
            3
        );
    }

    std::transform(
        modeName.begin(),
        modeName.end(),
        modeName.begin(),
        [](unsigned char c)
        {
            return (char)toupper(c);
        }
    );

    if (modeName == "MENU")
    {
        mode = MUSIC_MENU;

        return true;
    }

    if (modeName == "RACE")
    {
        mode = MUSIC_RACE;

        return true;
    }

    if (modeName == "ALL")
    {
        mode = MUSIC_ALL;

        return true;
    }

    printf(
        "[Music] INVALID MODE: '%s'\n",
        modeName.c_str()
    );

    return false;
}


// ============================================================
// LOAD INI
// ============================================================

static bool LoadMusicINI()
{
    g_MusicTracks.clear();

    std::string path =
        GetIniPath();

    std::ifstream file(path);

    if (!file.is_open())
    {
        printf(
            "[Music] Failed to open INI: %s\n",
            path.c_str()
        );

        return false;
    }

    bool inMusicSection = false;

    std::string line;

    while (std::getline(file, line))
    {
        line =
            Trim(line);

        if (line.empty())
            continue;

        if (
            line[0] == ';' ||
            line[0] == '#'
            )
        {
            continue;
        }

        if (line[0] == '[')
        {
            size_t close =
                line.find(']');

            if (close == std::string::npos)
            {
                inMusicSection = false;

                continue;
            }

            std::string section =
                line.substr(
                    1,
                    close - 1
                );

            section =
                Trim(section);

            inMusicSection =
                (
                    _stricmp(
                        section.c_str(),
                        "Music"
                    ) == 0
                    );

            continue;
        }

        if (!inMusicSection)
            continue;

        size_t equal =
            line.find('=');

        if (equal == std::string::npos)
            continue;

        std::string key =
            Trim(
                line.substr(
                    0,
                    equal
                )
            );

        std::string value =
            Trim(
                line.substr(
                    equal + 1
                )
            );

        if (
            _stricmp(
                key.c_str(),
                "ClosePursuitMusic"
            ) == 0
            )
        {
            g_ClosePursuitMusic =
                (atoi(value.c_str()) != 0);

            printf(
                "[Music] ClosePursuitMusic=%d\n",
                (int)g_ClosePursuitMusic
            );

            continue;
        }

        if (
            _strnicmp(
                key.c_str(),
                "Track",
                5
            ) != 0
            )
        {
            continue;
        }

        std::vector<std::string> fields =
            Split(
                value,
                '|'
            );

        if (fields.size() < 5)
        {
            printf(
                "[Music] Invalid track line: %s\n",
                line.c_str()
            );

            continue;
        }

        MusicTrack track{};

        char* endPtr = nullptr;

        unsigned long pathEvent =
            strtoul(
                fields[0].c_str(),
                &endPtr,
                10
            );

        if (
            endPtr ==
            fields[0].c_str()
            )
        {
            printf(
                "[Music] Invalid PathEvent: '%s'\n",
                fields[0].c_str()
            );

            continue;
        }

        track.PathEvent =
            (unsigned int)pathEvent;

        track.TrackName =
            fields[1];

        track.Artist =
            fields[2];

        track.Album =
            fields[3];

        if (
            !ParseMusicMode(
                fields[4],
                track.Mode
            )
            )
        {
            printf(
                "[Music] Skipping track '%s' "
                "because mode is invalid: '%s'\n",
                track.TrackName.c_str(),
                fields[4].c_str()
            );

            continue;
        }

        printf(
            "[Music] Loaded: %s | "
            "ModeText='%s' | Mode=%d\n",
            track.TrackName.c_str(),
            fields[4].c_str(),
            (int)track.Mode
        );

        g_MusicTracks.push_back(
            track
        );
    }

    file.close();

    printf(
        "[Music] Loaded %d tracks.\n",
        (int)g_MusicTracks.size()
    );

    return !g_MusicTracks.empty();
}


// ============================================================
// CHECK MODE
// ============================================================

static bool TrackMatchesMode(
    const MusicTrack& track,
    MusicMode requestedMode
)
{
    if (requestedMode == MUSIC_MENU)
    {
        return
            track.Mode == MUSIC_MENU ||
            track.Mode == MUSIC_ALL;
    }

    if (requestedMode == MUSIC_RACE)
    {
        return
            track.Mode == MUSIC_RACE ||
            track.Mode == MUSIC_ALL;
    }

    return true;
}


// ============================================================
// GET SHUFFLE BAG
// ============================================================

static std::vector<int>& GetShuffleBag(
    MusicMode mode
)
{
    if (mode == MUSIC_MENU)
        return g_MenuShuffleBag;

    return g_RaceShuffleBag;
}


// ============================================================
// SHUFFLE BAG
// ============================================================

static void ShuffleBag(
    std::vector<int>& bag
)
{
    if (bag.size() <= 1)
        return;

    for (
        int i = (int)bag.size() - 1;
        i > 0;
        i--
        )
    {
        int j =
            rand() % (i + 1);

        if (i != j)
        {
            std::swap(
                bag[i],
                bag[j]
            );
        }
    }
}


// ============================================================
// BUILD SHUFFLE BAG
// ============================================================

static void BuildShuffleBag(
    MusicMode mode
)
{
    std::vector<int>& bag =
        GetShuffleBag(mode);

    bag.clear();

    for (
        int i = 0;
        i < (int)g_MusicTracks.size();
        i++
        )
    {
        if (
            TrackMatchesMode(
                g_MusicTracks[i],
                mode
            )
            )
        {
            bag.push_back(i);
        }
    }

    ShuffleBag(bag);

    printf(
        "[Music] Built shuffle bag. "
        "Mode=%d Count=%d\n",
        (int)mode,
        (int)bag.size()
    );

    printf(
        "[Music] Shuffle order:"
    );

    for (
        size_t i = 0;
        i < bag.size();
        i++
        )
    {
        printf(
            " %d",
            bag[i]
        );
    }

    printf("\n");
}


// ============================================================
// GET RANDOM / SHUFFLED TRACK
// ============================================================

static int GetRandomTrack(
    MusicMode mode
)
{
    std::vector<int>& bag =
        GetShuffleBag(mode);

    if (bag.empty())
    {
        BuildShuffleBag(mode);
    }

    if (bag.empty())
        return -1;

    int index =
        bag.back();

    bag.pop_back();

    if (
        g_MusicTracks.size() > 1 &&
        index == g_CurrentTrack
        )
    {
        if (!bag.empty())
        {
            int alternative =
                bag.back();

            bag.pop_back();

            bag.push_back(index);

            ShuffleBag(bag);

            index =
                alternative;
        }
    }

    printf(
        "[Music] Shuffle pick: %d | %s | "
        "Remaining=%d | Mode=%d\n",
        index,
        g_MusicTracks[index].TrackName.c_str(),
        (int)bag.size(),
        (int)mode
    );

    return index;
}


// ============================================================
// PLAY TRACK
// ============================================================

static bool PlayTrack(int index)
{
    MusicTrack trackToPlay;
    void* targetSFXObj = nullptr;

    // --------------------------------------------------------
    // Kilit SADECE veriyi alıp durum bayraklarını sıfırlarken
    // tutulur.
    // --------------------------------------------------------
    {
        MusicLockGuard lock;

        if (
            index < 0 ||
            index >= (int)g_MusicTracks.size()
            )
        {
            return false;
        }

        if (!Current_SFXObj_PFEATrax)
        {
            printf(
                "[Music] Current SFXObj is null.\n"
            );

            return false;
        }

        trackToPlay = g_MusicTracks[index];
        targetSFXObj = Current_SFXObj_PFEATrax;

        g_CurrentTrack = index;

        g_DurationWasActive = false;
        g_TrackEndTriggered = false;
        g_WaitingForTrackStart = true;
        g_TrackStartWaitFrames = 0;
        g_TrackStartRetryCount = 0;
        g_TrackStartConfirmFrames = 0;
        g_FramesSinceTrackConfirmed = 0;
    }

    printf(
        "[Music] Playing: %s | %s | %s | PathEvent=%u\n",
        trackToPlay.TrackName.c_str(),
        trackToPlay.Artist.c_str(),
        trackToPlay.Album.c_str(),
        trackToPlay.PathEvent
    );

    // --------------------------------------------------------
    // Native motor çağrısı kilit DIŞINDA yapılır.
    // --------------------------------------------------------
    unsigned int result =
        StartLicensedMusic(
            targetSFXObj,
            trackToPlay.PathEvent
        );

    printf(
        "[Music] StartLicensedMusic: "
        "PathEvent=%u Return=%08X\n",
        trackToPlay.PathEvent,
        result
    );

    // --------------------------------------------------------
    // D3D Bildirimi kilit DIŞINDA yapılır (Deadlock'ı önler).
    // --------------------------------------------------------
    char message[512]{};

    if (!trackToPlay.Album.empty())
    {
        snprintf(
            message,
            sizeof(message),
            "%s\n%s",
            trackToPlay.Artist.c_str(),
            trackToPlay.Album.c_str()
        );
    }
    else
    {
        snprintf(
            message,
            sizeof(message),
            "%s",
            trackToPlay.Artist.c_str()
        );
    }

    SetNotify(
        trackToPlay.TrackName.c_str(),
        message,
        nullptr
    );

    return true;
}


// ============================================================
// PLAY RANDOM
// ============================================================

static void PlayRandomMusic(
    MusicMode mode
)
{
    if (g_PursuitMusicPaused)
        return;

    int index = -1;

    {
        MusicLockGuard lock;

        g_LastShuffleMode = mode;
        index = GetRandomTrack(mode);
    }

    if (index < 0)
    {
        printf(
            "[Music] No track available for mode %d.\n",
            (int)mode
        );

        return;
    }

    PlayTrack(index);
}


// ============================================================
// GET CURRENT MUSIC MODE
// ============================================================

static bool GetCurrentMusicMode(
    MusicMode& mode
)
{
    unsigned int state =
        *(unsigned int*)
        THEGAMEFLOWMANAGER_ADDRESS;

    if (state == 3)
    {
        mode = MUSIC_MENU;

        return true;
    }

    if (state == 6)
    {
        mode = MUSIC_RACE;

        return true;
    }

    return false;
}


// ============================================================
// NEXT
// ============================================================

static void PlayNextMusic()
{
    int nextIndex = -1;

    {
        MusicLockGuard lock;

        if (g_PursuitMusicPaused)
        {
            printf(
                "[Music] PlayNextMusic ignored: "
                "Pursuit pause active.\n"
            );

            return;
        }

        printf(
            "[Music] >>> PlayNextMusic CALLED <<< "
            "Track=%d DurationState=%d EndTriggered=%d Waiting=%d\n",
            g_CurrentTrack,
            (int)g_DurationWasActive,
            (int)g_TrackEndTriggered,
            (int)g_WaitingForTrackStart
        );

        if (g_MusicTracks.empty())
            return;

        MusicMode mode;

        if (!GetCurrentMusicMode(mode))
            return;

        printf(
            "[Music] PlayNextMusic: "
            "GameFlow=%u Mode=%d CurrentTrack=%d\n",
            *(DWORD*)THEGAMEFLOWMANAGER_ADDRESS,
            (int)mode,
            g_CurrentTrack
        );

        nextIndex =
            GetRandomTrack(mode);

        if (nextIndex < 0)
        {
            printf(
                "[Music] No candidates for mode %d.\n",
                (int)mode
            );

            return;
        }

        printf(
            "[Music] Shuffle next: "
            "%d -> %d | %s\n",
            g_CurrentTrack,
            nextIndex,
            g_MusicTracks[nextIndex].TrackName.c_str()
        );
    }

    // --------------------------------------------------------
    // PlayTrack kilit DIŞINDA çağrılır
    // --------------------------------------------------------
    if (!PlayTrack(nextIndex))
    {
        printf(
            "[Music] PlayTrack failed immediately.\n"
        );
    }
}


// ============================================================
// PREVIOUS
// ============================================================

static void PlayPreviousMusic()
{
    int previousIndex = -1;

    {
        MusicLockGuard lock;

        if (g_PursuitMusicPaused)
        {
            printf(
                "[Music] Previous ignored: "
                "Pursuit pause active.\n"
            );

            return;
        }

        if (g_MusicTracks.empty())
            return;

        MusicMode mode;

        if (!GetCurrentMusicMode(mode))
            return;

        std::vector<int> candidates;

        for (
            int i = 0;
            i < (int)g_MusicTracks.size();
            i++
            )
        {
            if (
                TrackMatchesMode(
                    g_MusicTracks[i],
                    mode
                )
                )
            {
                candidates.push_back(i);
            }
        }

        if (candidates.empty())
            return;

        int currentPosition = -1;

        for (
            int i = 0;
            i < (int)candidates.size();
            i++
            )
        {
            if (
                candidates[i] ==
                g_CurrentTrack
                )
            {
                currentPosition = i;

                break;
            }
        }

        if (currentPosition < 0)
        {
            previousIndex =
                candidates.back();
        }
        else
        {
            previousIndex =
                candidates[
                    (
                        currentPosition - 1 +
                        (int)candidates.size()
                        )
                        %
                        (int)candidates.size()
                ];
        }

        printf(
            "[Music] Previous track: %d -> %d | %s\n",
            g_CurrentTrack,
            previousIndex,
            g_MusicTracks[previousIndex].TrackName.c_str()
        );
    }

    // --------------------------------------------------------
    // PlayTrack kilit DIŞINDA çağrılır
    // --------------------------------------------------------
    if (previousIndex >= 0)
    {
        PlayTrack(previousIndex);
    }
}


// ============================================================
// KEY HANDLING
// ============================================================

static DWORD g_LastManualSkipTick = 0;

static const DWORD MIN_MS_BETWEEN_MANUAL_SKIPS = 250;


static void ManualNextMusic()
{
    if (g_PursuitMusicPaused)
    {
        printf(
            "[Music] F7 ignored: "
            "Pursuit pause active.\n"
        );

        return;
    }

    DWORD now =
        GetTickCount();

    if (
        now - g_LastManualSkipTick <
        MIN_MS_BETWEEN_MANUAL_SKIPS
        )
    {
        printf(
            "[Music] F7 ignored (debounce). "
            "DeltaMs=%lu\n",
            (unsigned long)
            (now - g_LastManualSkipTick)
        );

        return;
    }

    g_LastManualSkipTick = now;

    if (g_MusicChangeInProgress)
        return;

    g_MusicChangeInProgress = true;

    printf(
        "[Music] F7 -> Manual Next | CurrentTrack=%d\n",
        g_CurrentTrack
    );

    PlayNextMusic();

    g_DurationWasActive = false;

    g_TrackEndTriggered = false;

    g_MusicChangeInProgress = false;
}


static void UpdateMusicKeys()
{
    static bool previousF6 = false;

    static bool previousF7 = false;

    bool f6 =
        (GetAsyncKeyState(VK_F6) & 0x8000) != 0;

    bool f7 =
        (GetAsyncKeyState(VK_F7) & 0x8000) != 0;

    // ========================================================
    // F6
    // ========================================================

    if (
        f6 &&
        !previousF6
        )
    {
        if (
            !g_MusicChangeInProgress &&
            !g_PursuitMusicPaused
            )
        {
            g_MusicChangeInProgress = true;

            printf(
                "[Music] F6 -> Previous | CurrentTrack=%d\n",
                g_CurrentTrack
            );

            PlayPreviousMusic();

            g_DurationWasActive = false;

            g_TrackEndTriggered = false;

            g_MusicChangeInProgress = false;
        }
    }

    // ========================================================
    // F7
    // ========================================================

    if (
        f7 &&
        !previousF7
        )
    {
        ManualNextMusic();
    }

    previousF6 = f6;

    previousF7 = f7;
}


// ============================================================
// DISABLE INTERACTIVE - PURSUIT
// ============================================================

static bool InstallDisableInteractivePursuit()
{
    printf(
        "[Music] Installing DisableInteractive_Pursuit...\n"
    );

    WriteJump(
        DISABLEINTERACTIVEPURSUIT_ADDRESS1,
        (void*)DISABLEINTERACTIVE_FORCEEATRAX_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVEPURSUIT_ADDRESS2,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVEPURSUIT_ADDRESS3,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVEPURSUIT_ADDRESS4,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVEPURSUIT_ADDRESS5,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    WriteJump(
        DISABLEINTERACTIVEPURSUIT_ADDRESS6,
        (void*)DISABLEINTERACTIVE_FORCERETURN_ADDRESS
    );

    printf(
        "[Music] DisableInteractive_Pursuit installed.\n"
    );

    return true;
}


// ============================================================
// INITIALIZE
// ============================================================

void InitMenuMusic()
{
    if (g_MenuMusicInitialized)
        return;

    InitializeCriticalSection(
        &g_MusicLock
    );

    srand(
        GetTickCount()
    );

    WriteJump(
        STEALSFXOBJ_ADDRESS,
        StealSFXObj_Hook
    );

    LoadMusicINI();

    g_PreviousGameFlowState =
        0xFFFFFFFF;

    g_CurrentTrack =
        -1;

    g_MusicSFXObj =
        0;

    g_DurationWasActive =
        false;

    g_TrackEndTriggered =
        false;

    g_MusicChangeInProgress =
        false;

    g_WaitingForTrackStart =
        false;

    g_TrackStartWaitFrames =
        0;

    g_TrackStartRetryCount =
        0;

    g_TrackStartConfirmFrames =
        0;

    g_FramesSinceTrackConfirmed =
        0;

    g_PendingMusicStart =
        false;

    g_PendingMusicMode =
        MUSIC_MENU;

    g_PendingMusicDelay =
        0;

    g_LastManualSkipTick =
        0;

    g_PursuitMusicPaused =
        false;

    g_TrackBeforePursuit =
        -1;

    g_MenuShuffleBag.clear();

    g_RaceShuffleBag.clear();

    g_LastShuffleMode =
        MUSIC_ALL;

    if (g_ClosePursuitMusic)
    {
        InstallDisableInteractivePursuit();
    }

    InstallDisableInteractiveCrew();

    InstallDisableInteractiveCanyon();

    InstallMusicUpdateHook();

    g_MenuMusicInitialized =
        true;

    printf(
        "[Music] MenuMusic initialized.\n"
    );
}


// ============================================================
// SHUTDOWN
// ============================================================

void ShutdownMenuMusic()
{
    if (!g_MenuMusicInitialized)
        return;

    DeleteCriticalSection(
        &g_MusicLock
    );

    g_MenuMusicInitialized =
        false;

    printf(
        "[Music] MenuMusic shut down.\n"
    );
}


// ============================================================
// UPDATE
// ============================================================

void UpdateMenuMusic()
{
    UpdateMusicKeys();

    DWORD gameFlow =
        *(DWORD*)THEGAMEFLOWMANAGER_ADDRESS;

    if (g_PursuitMusicPaused)
        return;

    // ========================================================
    // STATE DEĞİŞTİ
    // ========================================================

    if (
        gameFlow !=
        g_PreviousGameFlowState
        )
    {
        printf(
            "[Music] GameFlow changed: %u -> %u\n",
            g_PreviousGameFlowState,
            gameFlow
        );

        g_PreviousGameFlowState =
            gameFlow;

        // ----------------------------------------------------
        // MENU
        // ----------------------------------------------------

        if (gameFlow == 3)
        {
            printf(
                "[Music] Entering MENU.\n"
            );

            g_PendingMusicStart =
                true;

            g_PendingMusicMode =
                MUSIC_MENU;

            g_PendingMusicDelay =
                2;

            return;
        }

        // ----------------------------------------------------
        // RACE
        // ----------------------------------------------------

        if (gameFlow == 6)
        {
            printf(
                "[Music] Entering RACE.\n"
            );

            g_PendingMusicStart =
                true;

            g_PendingMusicMode =
                MUSIC_RACE;

            g_PendingMusicDelay =
                5;

            return;
        }

        // ----------------------------------------------------
        // Other state
        // ----------------------------------------------------

        g_PendingMusicStart =
            false;

        g_PendingMusicDelay =
            0;

        g_WaitingForTrackStart =
            false;

        g_TrackStartWaitFrames =
            0;

        g_TrackStartRetryCount =
            0;

        g_TrackStartConfirmFrames =
            0;

        g_FramesSinceTrackConfirmed =
            0;

        g_DurationWasActive =
            false;

        g_TrackEndTriggered =
            false;

        return;
    }

    // ========================================================
    // PENDING MUSIC
    // ========================================================

    if (g_PendingMusicStart)
    {
        if (g_PursuitMusicPaused)
            return;

        if (g_PendingMusicDelay > 0)
        {
            g_PendingMusicDelay--;

            return;
        }

        if (!Current_SFXObj_PFEATrax)
        {
            printf(
                "[Music] Waiting for SFXObj...\n"
            );

            return;
        }

        printf(
            "[Music] Starting pending music. "
            "SFXObj=%08X Mode=%d\n",
            (unsigned int)
            (uintptr_t)Current_SFXObj_PFEATrax,
            (int)g_PendingMusicMode
        );

        g_PendingMusicStart =
            false;

        PlayRandomMusic(
            g_PendingMusicMode
        );
    }
}