#include "imgui-1.92.5/imgui.h"
#include "imgui-1.92.5/backends/imgui_impl_dx9.h"
#include "imgui-1.92.5/backends/imgui_impl_win32.h"
#include "Patches.h"
#include "Console.h"
#include "d3d9.h"
#include <d3dx9.h> 
#include <string>
#include <vector>
#include <windows.h>
#include <fstream>
#include <iostream>
#include "mmsystem.h"
#pragma comment(lib, "C:/Program Files (x86)/directxsdk/Lib/x86/d3dx9.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "winmm.lib")
#include "DALCareer.h"
#include "detours.h"
#include <cstring>
#include "RaceInfo.h"
#include "WorldObjectCreator.h"
enum class ERaceType : int
{
    Speedtrap = 0,
    Sprint,
    Circuit,
    LapKnockout,
    Tollbooth,
    TollboothCircuit,
    RaceWars,
    HighwayBattle,
    Count
};

static const char* kRaceTypeIconPaths[(int)ERaceType::Count] =
{
    "GLOBAL\\events\\race_icons\\speedtrap.png",
    "GLOBAL\\events\\race_icons\\sprint.png",
    "GLOBAL\\events\\race_icons\\circuit.png",
    "GLOBAL\\events\\race_icons\\lap_knockout.png",
    "GLOBAL\\events\\race_icons\\tollbooth.png",
    "GLOBAL\\events\\race_icons\\tollbooth_circuit.png",
    "GLOBAL\\events\\race_icons\\race_wars.png",
    "GLOBAL\\events\\race_icons\\highway_battle.png",
};

static LPDIRECT3DTEXTURE9 gRaceTypeTextures[(int)ERaceType::Count] = {};
static bool gRaceTypeTexturesLoaded = false;

struct RaceDef
{
    uint32_t hash;
    ERaceType type;
    const char* title;     // kart başlığı (ör: "Sprint")
    const char* location;  // alt başlık (ör: "Times Meydanı")
};

static const RaceDef kRaceDefs[10] =
{
    { 0x78C7FA24, ERaceType::Sprint,        "SPRINT",            u8"Times Meydanı" },
    { 0x00000000, ERaceType::Circuit,       "CIRCUIT",           u8"Downtown Loop" },
    { 0x00000000, ERaceType::Speedtrap,     "SPEEDTRAP",         u8"Port District" },
    { 0x00000000, ERaceType::LapKnockout,   "LAP KNOCKOUT",      u8"West Park" },
    { 0x00000000, ERaceType::Tollbooth,     "TOLLBOOTH",         u8"Industrial" },
    { 0x00000000, ERaceType::TollboothCircuit,"TOLLBOOTH CIRCUIT",u8"River Run" },
    { 0x00000000, ERaceType::RaceWars,      "RACE WARS",         u8"Stadium" },
    { 0x00000000, ERaceType::HighwayBattle, "HIGHWAY BATTLE",    u8"Expressway" },
    { 0x00000000, ERaceType::Sprint,        "SPRINT",            u8"Harbor Line" },
    { 0x00000000, ERaceType::Circuit,       "CIRCUIT",           u8"Uptown Ring" },
};

struct RaceNode
{
    int index = 0;
    uint32_t raceHash = 0;
    ERaceType type = ERaceType::Sprint;

    std::string title;   // "SPRINT"
    std::string location;// "Times Meydanı"

    bool unlocked = false;
    bool completed = false;
};

static std::vector<RaceNode> gRaceNodes;
static bool gRewardGranted = false;

// Ödül araba adı
static const char* gRewardCarName = "ANGIE";

// HASH LIST
static const uint32_t kRaceHashes[10] =
{
    0x78C7FA24, // 1
    0x00000000, // 2
    0x00000000, // 3
    0x00000000, // 4
    0x00000000, // 5
    0x00000000, // 6
    0x00000000, // 7
    0x00000000, // 8
    0x00000000, // 9
    0x00000000, // 10
};

static void InitRaceChainIfNeeded();
static void UpdateRaceChainStates();
static void DrawRaceChainGridUI();
static void DrawLockOverlay(const ImVec2& cardMin, const ImVec2& cardMax);
static void OnRaceCardClicked(int i0);
extern LPDIRECT3DDEVICE9 g_pd3dDevice;
// -------------------------------------------------------
static LPDIRECT3DTEXTURE9 LoadTextureFromFile(LPDIRECT3DDEVICE9 pDevice, const char* path)
{
    if (!pDevice || !path || !path[0]) return nullptr;

    LPDIRECT3DTEXTURE9 tex = nullptr;

    D3DXIMAGE_INFO info{};
    HRESULT hrInfo = D3DXGetImageInfoFromFileA(path, &info);
    if (FAILED(hrInfo)) {
        MessageBoxA(0, path, "D3DXGetImageInfoFromFileA FAILED", 0);
        return nullptr;
    }

    HRESULT hr = D3DXCreateTextureFromFileExA(
        pDevice,
        path,
        info.Width,
        info.Height,
        1,
        0,
        D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED,
        D3DX_FILTER_LINEAR,
        D3DX_FILTER_LINEAR,
        0,
        &info,
        nullptr,
        &tex
    );

    if (FAILED(hr)) return nullptr;
    return tex;
}

static void LoadRaceTypeTexturesIfNeeded()
{
    if (gRaceTypeTexturesLoaded) return;
    if (!g_pd3dDevice) return; // device hazır değilse bekle

    for (int i = 0; i < (int)ERaceType::Count; i++)
    {
        const char* path = kRaceTypeIconPaths[i];
        gRaceTypeTextures[i] = LoadTextureFromFile(g_pd3dDevice, path);
    }

    gRaceTypeTexturesLoaded = true;
}
static char g_CarInput[64] = "";


LPDIRECT3DTEXTURE9 millionaireIcon = nullptr;
LPDIRECT3DTEXTURE9 g_CurrentAchievementTexture = nullptr;

bool show_menu = false;
bool infinite_nos = false;
bool ghost_car = false;
bool testdialog = false;
bool g_NotifyShow = false;
float g_NotifyTimer = 0.0f;
char g_NotifyAlbum[128] = "";
char g_NotifyTitle[128] = "";
char g_NotifyMsg[256] = "";
bool forcepursuit = false;
bool forcecheckpoint = false;

void SetNotify(
    const char* title,
    const char* msg,
    LPDIRECT3DTEXTURE9 tex
)
{
    strncpy_s(
        g_NotifyTitle,
        title ? title : "",
        _TRUNCATE
    );

    strncpy_s(
        g_NotifyMsg,
        msg ? msg : "",
        _TRUNCATE
    );

    // Yeni bildirim geldiğinde animasyonu sıfırla
    g_CurrentAchievementTexture = tex;
    g_NotifyTimer = 0.0f;
    g_NotifyShow = true;
}


void RenderNotification()
{
    if (!g_NotifyShow)
        return;

    ImGuiIO& io = ImGui::GetIO();
    g_NotifyTimer += io.DeltaTime;

    const float width = 350.0f;
    const float height = 100.0f;
    const float animTime = 0.5f;
    const float totalTime = 4.5f;

    // Süre dolduysa stilleri push etmeden önce doğrudan kapatıp çık
    if (g_NotifyTimer >= totalTime)
    {
        g_NotifyShow = false;
        return;
    }

    float x = -width;
    if (g_NotifyTimer < animTime)
    {
        float t = g_NotifyTimer / animTime;
        t = t * t * (3.0f - 2.0f * t);
        x = -width + (width + 20.0f) * t;
    }
    else if (g_NotifyTimer < totalTime - animTime)
    {
        x = 20.0f;
    }
    else
    {
        float t = (g_NotifyTimer - (totalTime - animTime)) / animTime;
        t = t * t * (3.0f - 2.0f * t);
        x = 20.0f - (width + 20.0f) * t;
    }

    ImGui::SetNextWindowPos(ImVec2(x, 45.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.08f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.03f, 0.94f, 0.84f, 0.7f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

    if (ImGui::Begin(
        "##MusicChyron",
        nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoInputs
    ))
    {
        ImVec2 startCursorPos = ImGui::GetCursorPos();
        ImVec2 screenPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        float logoSize = 80.0f;
        float radius = logoSize / 2.0f;
        ImVec2 center(screenPos.x + radius, screenPos.y + radius);

        float spinSpeed = 4.0f;
        float angleMin = g_NotifyTimer * spinSpeed;
        float angleMax = angleMin + (3.14159265f * 1.25f);

        drawList->PathArcTo(center, radius + 4.0f, angleMin, angleMax, 32);
        drawList->PathStroke(ImGui::GetColorU32(ImVec4(0.03f, 0.94f, 0.84f, 1.0f)), false, 3.0f);
        drawList->AddCircleFilled(center, radius, IM_COL32(20, 20, 20, 255));
        ImGui::Dummy(ImVec2(logoSize, logoSize));

        ImGui::SetCursorPos(ImVec2(startCursorPos.x + logoSize + 25.0f, startCursorPos.y + 12.0f));
        ImGui::BeginGroup();

        ImGui::TextColored(ImVec4(0.03f, 0.94f, 0.84f, 1.0f), "%s", g_NotifyTitle);
        ImGui::Text("%s", g_NotifyMsg);
        ImGui::TextColored(ImVec4(0.65f, 0.68f, 0.70f, 1.0f), "%s", g_NotifyAlbum);

        ImGui::EndGroup();
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}



static void InitRaceChainIfNeeded()
{
    if (!gRaceNodes.empty()) return;

    LoadRaceTypeTexturesIfNeeded(); // icon cache

    gRaceNodes.reserve(10);

    for (int i = 0; i < 10; i++)
    {
        const RaceDef& d = kRaceDefs[i];

        RaceNode n;
        n.index = i + 1;
        n.raceHash = d.hash;
        n.type = d.type;
        n.title = d.title ? d.title : "RACE";
        n.location = d.location ? d.location : "";

        n.unlocked = (i == 0);
        n.completed = false;

        gRaceNodes.push_back(std::move(n));
    }

    gRewardGranted = false;
}

static void UpdateRaceChainStates()
{
    if (gRaceNodes.empty()) return;
    for (auto& r : gRaceNodes)
    {
        if (r.raceHash == 0) { r.completed = false; continue; }
        uint8_t flags = 0;
        bool done = RaceTools::IsRaceCompleted(r.raceHash, &flags);
        r.completed = done;
    }
    gRaceNodes[0].unlocked = true;
    for (int i = 1; i < (int)gRaceNodes.size(); i++)
    {
        gRaceNodes[i].unlocked = gRaceNodes[i - 1].completed;
    }
    bool allDone = true;
    for (auto& r : gRaceNodes)
    {
        if (!r.completed) { allDone = false; break; }
    }

    if (allDone && !gRewardGranted)
    {
        gRewardGranted = true;
        RaceTools::WriteStatEncrypted("reward_granted", 0, true, 0);
        NFSC::AchievementCarManager::QueueAddCarByName(gRewardCarName);
    }
}

static void DrawLockOverlay(const ImVec2& cardMin, const ImVec2& cardMax)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(cardMin, cardMax, IM_COL32(0, 0, 0, 140), 10.0f);
}

static void OnRaceCardClicked(int i0)
{
    if (i0 < 0 || i0 >= (int)gRaceNodes.size()) return;
    RaceNode& n = gRaceNodes[i0];

    if (!n.unlocked)
    {
        return;
    }

    if (n.raceHash == 0)
    {
        return;
    }

    uint8_t flags = 0;
    bool done = RaceTools::IsRaceCompleted(n.raceHash, &flags);

    if (done)
    {
        char tag[64]{};
        std::snprintf(tag, sizeof(tag), "race_%d_completed", n.index);
        RaceTools::WriteStatEncrypted(tag, n.raceHash, true, flags);

    }
    else
    {
        RaceTools::RaceStart(n.raceHash, 7);
    }
    UpdateRaceChainStates();
}

static void DrawRaceChainGridUI()
{
    InitRaceChainIfNeeded();
    UpdateRaceChainStates();

    ImGui::TextColored(ImVec4(0.03f, 0.94f, 0.84f, 1.0f), u8"Güncel Etkinlik");
    ImGui::BeginChild("RaceChainScroll", ImVec2(0, 220), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    const float cardW = 200.0f;
    const float cardH = 90.0f;
    const float pad = 10.0f;

    float avail = ImGui::GetContentRegionAvail().x;
    int cols = (int)(avail / (cardW + pad));
    if (cols < 1) cols = 1;

    for (int i = 0; i < (int)gRaceNodes.size(); i++)
    {
        RaceNode& n = gRaceNodes[i];
        ImGui::PushID(i);

        if (i % cols != 0) ImGui::SameLine(0.0f, pad);

        ImVec2 start = ImGui::GetCursorScreenPos();
        ImVec2 cardMin = start;
        ImVec2 cardMax = ImVec2(start.x + cardW, start.y + cardH);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImU32 bg = n.completed ? IM_COL32(20, 90, 40, 180) : IM_COL32(30, 30, 35, 180);
        dl->AddRectFilled(cardMin, cardMax, bg, 10.0f);
        dl->AddRect(cardMin, cardMax, IM_COL32(80, 80, 90, 200), 10.0f);

        ImGui::InvisibleButton("race_card", ImVec2(cardW, cardH));
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        float x = cardMin.x + 10.0f;
        float y = cardMin.y + 10.0f;
        LPDIRECT3DTEXTURE9 iconTex = gRaceTypeTextures[(int)n.type];


        if (iconTex)
            dl->AddImage((ImTextureID)iconTex, ImVec2(x, y), ImVec2(x + 40, y + 40));
        else
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 40, y + 40), IM_COL32(60, 60, 70, 220), 6.0f);

        dl->AddText(ImVec2(x + 50, y), IM_COL32(255, 255, 255, 240), n.title.c_str());
        dl->AddText(ImVec2(x + 50, y + 38), IM_COL32(200, 200, 200, 220), n.location.c_str());

        const char* st = n.completed ? u8"[TAMAMLANDI]" : (n.unlocked ? u8"[AÇIK]" : u8"[KİLİTLİ]");
        ImU32 stc = n.completed ? IM_COL32(0, 255, 120, 255) : (n.unlocked ? IM_COL32(255, 220, 80, 255) : IM_COL32(255, 80, 80, 255));
        dl->AddText(ImVec2(x + 50, y + 20), stc, st);

        if (!n.unlocked)
            DrawLockOverlay(cardMin, cardMax);

        if (clicked)
            OnRaceCardClicked(i);

        ImGui::PopID();
    }

    ImGui::EndChild();
}

void RenderMenu(LPDIRECT3DDEVICE9 pDevice)
{
    ImGuiIO& io = ImGui::GetIO();

    // Menü toggle tuşu (Numpad1)
    if (GetAsyncKeyState(VK_NUMPAD1) & 1)
        show_menu = !show_menu;

    // Menü veya popup açık mı?
    bool anyUIOpen = show_menu;

    // ImGui input ayarları
    io.MouseDrawCursor = anyUIOpen;
    io.WantCaptureMouse = anyUIOpen;
    io.WantCaptureKeyboard = anyUIOpen;

    // Popup modal açıksa sadece popup render et

    // Menü açık ise render et
    if (show_menu)
    {
        ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("NFSPGP Debug Menu", &show_menu))
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Fonksiyonlar");
            ImGui::Separator();

            if (ImGui::Checkbox(u8"Sınırsız NOS", &infinite_nos)) SetInfiniteNos(infinite_nos);
			if (ImGui::Checkbox(u8"Her zaman CheckpointVisible'ı aktif et", &forcecheckpoint)) SetForceCheckpointVisible(forcecheckpoint);
			if (ImGui::Checkbox(u8"Pursuit Başlat", &forcepursuit)) Game::GameForcePursuitStart(forcepursuit);

            static char g_CarInput[64] = "";
            ImGui::InputText("Preset XName", g_CarInput, IM_ARRAYSIZE(g_CarInput));
            if (ImGui::Button("Araba Ekle"))
            {
                if (!NFSC::AchievementCarManager::HasCarDB())
                {
                    return;
                }
                if (g_CarInput[0] == '\0')
                {
                    return;
                }
                NFSC::AchievementCarManager::QueueAddCarByName(g_CarInput);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // --- YARIŞ ZİNCİRİ GRID ---
            DrawRaceChainGridUI();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(
                ImVec4(
                    0.03f,
                    0.94f,
                    0.84f,
                    1.0f
                ),
                "WorldModel HOT Test"
            );

            // ============================================================
            // Model Hash
            // ============================================================

            static char g_WorldHashInput[32] =
                "738B1F9B";

            ImGui::InputText(
                "Model Hash",
                g_WorldHashInput,
                IM_ARRAYSIZE(g_WorldHashInput)
            );

            // ============================================================
            // HOT Index
            // ============================================================

            static int g_HotPositionIndex = 0;

            ImGui::InputInt(
                "HOT Position Index",
                &g_HotPositionIndex
            );

            if (g_HotPositionIndex < 0)
                g_HotPositionIndex = 0;

            // ============================================================
            // Rotation
            // ============================================================

            static float g_WorldRotX = 0.0f;
            static float g_WorldRotY = 0.0f;
            static float g_WorldRotZ = 0.0f;

            ImGui::Text("Rotation (Degrees)");

            ImGui::InputFloat(
                "Rotation X",
                &g_WorldRotX,
                1.0f,
                10.0f,
                "%.2f"
            );

            ImGui::InputFloat(
                "Rotation Y",
                &g_WorldRotY,
                1.0f,
                10.0f,
                "%.2f"
            );

            ImGui::InputFloat(
                "Rotation Z",
                &g_WorldRotZ,
                1.0f,
                10.0f,
                "%.2f"
            );

            // ============================================================
            // Spawn
            // ============================================================

            if (ImGui::Button(
                "WorldModel Spawn",
                ImVec2(180, 35)))
            {
                uint32_t hash =
                    static_cast<uint32_t>(
                        strtoul(
                            g_WorldHashInput,
                            nullptr,
                            16
                        )
                        );

                SpawnWorldModelFromHotPosition(
                    hash,
                    g_HotPositionIndex,

                    g_WorldRotX,
                    g_WorldRotY,
                    g_WorldRotZ
                );
            }

            // ============================================================
            // Status
            // ============================================================

            if (GetTestWorldModel())
            {
                ImGui::SameLine();

                ImGui::TextColored(
                    ImVec4(
                        0.0f,
                        1.0f,
                        0.3f,
                        1.0f
                    ),
                    "Spawned: %p",
                    GetTestWorldModel()
                );
            }

            // --- BAŞARIMLAR --

            ImGui::Spacing();
            if (ImGui::Button("Kapat", ImVec2(120, 35)))
                show_menu = false;
        }
        ImGui::End();
    }
}