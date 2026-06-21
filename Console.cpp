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
#define JSON_HAS_CPP_20 0
#include "json.hpp"
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

static void SetNotify(const char* title, const char* msg, LPDIRECT3DTEXTURE9 tex = nullptr);
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

extern std::vector<Achievement> myAchievements;
static char g_CarInput[64] = "";


std::vector<Achievement> myAchievements = {
    {(const char*)u8"MİLYONER", (const char*)u8"Kariyerde 1.000.000$ para kazan.", "GLOBAL\\achievement\\money.png", false, nullptr},
    {(const char*)u8"HIZ TUTKUNU", (const char*)u8"300 KM/H hızı geçtin.", "GLOBAL\\achievement\\speed.png", false, nullptr},
    {(const char*)u8"KANUN KAÇAĞI", (const char*)u8"50 polisi pert ettin.", "GLOBAL\\achievement\\cop.png", false, nullptr}
};

void PlayAchievementSound() {
    mciSendStringA("close achievement_sound", NULL, 0, NULL);
    mciSendStringA("open \"GLOBAL\\achievement\\completed.mp3\" type mpegvideo alias achievement_sound", NULL, 0, NULL);
    mciSendStringA("play achievement_sound", NULL, 0, NULL);
}

LPDIRECT3DTEXTURE9 millionaireIcon = nullptr;
LPDIRECT3DTEXTURE9 g_CurrentAchievementTexture = nullptr;

bool show_menu = false;
bool infinite_nos = false;
bool ghost_car = false;
bool testdialog = false;
bool g_NotifyShow = false;
float g_NotifyTimer = 0.0f;
char g_NotifyTitle[128] = "";
char g_NotifyMsg[256] = "";

static void SetNotify(const char* title, const char* msg, LPDIRECT3DTEXTURE9 tex)
{
    if (g_NotifyShow) return;
    strncpy_s(g_NotifyTitle, title ? title : "", _TRUNCATE);
    strncpy_s(g_NotifyMsg, msg ? msg : "", _TRUNCATE);
    g_CurrentAchievementTexture = tex;
    g_NotifyTimer = 0.0f;
    g_NotifyShow = true;
}

void RenderNotification() {
    if (!g_NotifyShow) return;

    static int s_lastFrame = -1;
    int frame = ImGui::GetFrameCount();
    if (frame == s_lastFrame) return;
    s_lastFrame = frame;

    ImGuiIO& io = ImGui::GetIO();
    g_NotifyTimer += io.DeltaTime;

    float x_pos = -400.0f;
    float total_time = 4.0f;
    float anim_speed = 0.6f;

    if (g_NotifyTimer < anim_speed) {
        float t = g_NotifyTimer / anim_speed;
        x_pos = -400.0f + (t * 420.0f);
    }
    else if (g_NotifyTimer < (total_time - anim_speed)) {
        x_pos = 20.0f;
    }
    else if (g_NotifyTimer < total_time) {
        float t = (g_NotifyTimer - (total_time - anim_speed)) / anim_speed;
        x_pos = 20.0f - (t * 420.0f);
    }
    else {
        g_NotifyShow = false;
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(x_pos, 50.0f));
    ImGui::SetNextWindowSize(ImVec2(600, 0));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 0.94f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.03f, 0.94f, 0.84f, 0.6f));

    ImGui::Begin("##GlobalNotify", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav);

    if (g_CurrentAchievementTexture != nullptr) {
        ImGui::Image((void*)g_CurrentAchievementTexture, ImVec2(55, 55));
        ImGui::SameLine();
    }

    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(0.03f, 0.94f, 0.84f, 1.0f), g_NotifyTitle);
    ImGui::Separator();
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 280.0f);
    ImGui::Text(g_NotifyMsg);
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();

    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void SaveAchievementsSecure() {
    std::ofstream file("GLOBAL\\stats.dat", std::ios::binary);
    if (!file.is_open()) return;

    size_t count = myAchievements.size();
    file.write((char*)&count, sizeof(count));

    for (const auto& ach : myAchievements) {
        file.write((char*)&ach.unlocked, sizeof(bool));
    }

    int magicKey = 0xDEADC0DE;
    file.write((char*)&magicKey, sizeof(magicKey));
    file.close();
}

void LoadAchievementsSecure() {
    std::ifstream file("GLOBAL\\stats.dat", std::ios::binary);
    if (!file.is_open()) return;

    size_t savedCount;
    file.read((char*)&savedCount, sizeof(savedCount));

    if (savedCount != myAchievements.size()) {
        file.close();
        return;
    }

    for (auto& ach : myAchievements) {
        file.read((char*)&ach.unlocked, sizeof(bool));
    }

    int readKey;
    file.read((char*)&readKey, sizeof(readKey));

    if (readKey != 0xDEADC0DE) {
        MessageBoxA(NULL, "Başarım dosyayında değişiklik yapıldığı tespit edildi. Tüm başarımlar sıfırlandı.", "Hata", MB_OK | MB_ICONERROR);
        for (auto& ach : myAchievements) ach.unlocked = false;
    }

    file.close();
}

void TriggerAchievement(int index) {
    if (index < 0 || (size_t)index >= myAchievements.size()) return;
    if (g_NotifyShow) return;
    if (myAchievements[index].unlocked) return;

    myAchievements[index].unlocked = true;
    SaveAchievementsSecure();

    PlayAchievementSound();

    strncpy_s(g_NotifyTitle, myAchievements[index].name.c_str(), _TRUNCATE);
    strncpy_s(g_NotifyMsg, myAchievements[index].description.c_str(), _TRUNCATE);
    g_CurrentAchievementTexture = myAchievements[index].texture;

    g_NotifyTimer = 0.0f;
    g_NotifyShow = true;
}

static int lastCash = -1;
void AchievementUpdate()
{
    if (!DALCareer::Game_IsCareerMode())
        return;

    int cash = 0;
    if (!DALCareer::GetCash(&cash))
        return;

    if (cash == lastCash)
        return;

    lastCash = cash;

    if (cash >= 10000 && !myAchievements[0].unlocked)
        TriggerAchievement(0);


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

        SetNotify("ÖDÜL!", u8"Tüm yarışlar başarıyla tamamlandı!, Araç garajınıza teslim edilecektir.", nullptr);
        PlayAchievementSound();
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
        SetNotify(u8"KİLİTLİ", u8"Önce bir önceki yarışı tamamlamanız gerekiyor", nullptr);
        return;
    }

    if (n.raceHash == 0)
    {
        SetNotify("HATA", u8"Bu yarisin hash'i ayarlanmamis!", nullptr);
        return;
    }

    uint8_t flags = 0;
    bool done = RaceTools::IsRaceCompleted(n.raceHash, &flags);

    if (done)
    {
        char tag[64]{};
        std::snprintf(tag, sizeof(tag), "race_%d_completed", n.index);
        RaceTools::WriteStatEncrypted(tag, n.raceHash, true, flags);

        SetNotify("TAMAMLANDI", u8"Yarış başarıyla tamamlandı", nullptr);
        PlayAchievementSound();
    }
    else
    {
        SetNotify(u8"Etkinlik", u8"Seçili yarış başlatılıyor.", nullptr);
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

bool g_RestoreMenuVisible = false;
struct CarForSale
{
    std::string presetName;
    std::string displayName;
    int price;
    bool purchased;
    bool restored;
    bool caradded;
    LPDIRECT3DTEXTURE9 texture;
};

std::string XorEncryptDecrypt(const std::string& data, const std::string& key)
{
    std::string result = data;
    for (size_t i = 0; i < data.size(); i++)
        result[i] ^= key[i % key.size()];
    return result;
}

// Örnek araç listesi
static std::vector<CarForSale> g_CarsForSale = {
    { "FALCONXY", "Ford Falcon XY GT-HO Phase II", 50000, false, false, false, nullptr },
};

static void LoadCarTextures(LPDIRECT3DDEVICE9 pDevice)
{
    for (auto& car : g_CarsForSale)
    {
        if (!car.texture && !car.presetName.empty()) // presetName boş değilse
        {
            // Tam dosya yolunu oluştur
            std::string path = "GLOBAL\\cars\\" + car.presetName + ".jpg";

            // Texture yükle
            car.texture = LoadTextureFromFile(pDevice, path.c_str());

            if (!car.texture)
            {
                // debug için mesaj
                char msg[256];
                snprintf(msg, sizeof(msg), "Failed to load car texture: %s", path.c_str());
                MessageBoxA(NULL, msg, "Texture Load Error", MB_OK);
            }
        }
    }
}
using json = nlohmann::json;
void SaveCarPurchases()
{
    json j;

    for (size_t i = 0; i < g_CarsForSale.size(); i++)
    {
        j["cars"][i]["purchased"] = g_CarsForSale[i].purchased;
        j["cars"][i]["restored"] = g_CarsForSale[i].restored;
    }

    std::string data = j.dump();
    std::string encrypted = XorEncryptDecrypt(data, "my_secret_key");

    std::ofstream file("GLOBAL\\cars.json", std::ios::binary);
    file << encrypted;
    file.close();
}

void LoadCarPurchases()
{
    std::ifstream file("GLOBAL\\cars.json", std::ios::binary);
    if (!file.is_open()) return;

    std::string encrypted((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    std::string decrypted = XorEncryptDecrypt(encrypted, "my_secret_key");

    json j = json::parse(decrypted, nullptr, false);
    if (j.is_discarded()) return;

    for (size_t i = 0; i < g_CarsForSale.size(); i++)
    {
        g_CarsForSale[i].purchased = j["cars"][i]["purchased"];
        g_CarsForSale[i].restored = j["cars"][i]["restored"];
    }
}

bool g_ShowCarShop = false;
void DrawCarShopMenu(LPDIRECT3DDEVICE9 pDevice)
{
    if (!g_ShowCarShop) return;

    // Modern stil ayarları
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.FrameRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.WindowPadding = ImVec2(15, 15);

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 0.95f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.23f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.22f, 0.45f, 0.85f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.55f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.40f, 0.80f, 1.0f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.45f, 0.85f, 1.0f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.30f, 0.55f, 0.95f, 1.0f);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::Begin(u8"Güner Production Restore Garajı##CarShop", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

    LoadCarTextures(pDevice);

    int cash = 0;
    DALCareer::GetCash(&cash);

    ImGui::Text(u8"Menü");

    if (ImGui::BeginTabBar("CarMenuTabs", ImGuiTabBarFlags_FittingPolicyScroll))
    {
        // ------------------- SATIN AL -------------------
        if (ImGui::BeginTabItem(u8"ED'IN HURDALIĞI"))
        {
            ImGui::BeginChild("ShopScroll", ImVec2(0, 0), true);

            for (size_t i = 0; i < g_CarsForSale.size(); i++)
            {
                auto& car = g_CarsForSale[i];
                ImGui::PushID((int)i);

                // Araç kartı
                ImGui::BeginChildFrame(ImGui::GetID(("CarFrame" + std::to_string(i)).c_str()), ImVec2(0, 350), ImGuiWindowFlags_NoScrollbar);

                ImVec2 windowSize = ImGui::GetContentRegionAvail();

                // Resmi ortala
                if (car.texture)
                {
                    ImVec2 texSize(512, 256);
                    ImVec2 pos = ImVec2((windowSize.x - texSize.x) * 0.5f, 0);
                    ImGui::SetCursorPos(pos);
                    ImGui::Image(car.texture, texSize);


                    // Resim için border
                    ImVec2 pMin = ImGui::GetItemRectMin();
                    ImVec2 pMax = ImGui::GetItemRectMax();
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 255), 8.0f, 0, 2.0f);
                }

                // Araç ismini ortala
                std::string carName = car.displayName;
                ImVec2 textSize = ImGui::CalcTextSize(carName.c_str());
                ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);
                ImGui::Text("%s", carName.c_str());

                // Satın Al butonu ortala
                ImVec2 buttonSize(120, 25);
                ImGui::SetCursorPosX((windowSize.x - buttonSize.x) * 0.5f);

                if (!car.purchased)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.6f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.5f, 0.15f, 1.0f));

                    if (ImGui::Button(u8"Satın Al", buttonSize))
                    {
                        if (cash >= car.price)
                        {
                            DALCareer::SetCash(cash - car.price);
                            car.purchased = true;
                            SaveCarPurchases();
                        }
                        else SetNotify(u8"Yetersiz Para", u8"Bu aracı almak için yeterli paranız yok!", nullptr);
                    }

                    ImGui::PopStyleColor(3);
                }
                else
                {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), u8"SATIN ALINDI");
                }

                ImGui::EndChildFrame();
                ImGui::PopID();
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // ------------------- RESTORE -------------------
        if (ImGui::BeginTabItem(u8"Güner Production Garajı"))
        {
            ImGui::BeginChild("RestoreScroll", ImVec2(0, 0), true);

            for (size_t i = 0; i < g_CarsForSale.size(); i++)
            {
                auto& car = g_CarsForSale[i];
                ImGui::PushID((int)i);

                if (car.purchased)
                {
                    int restoreCost = car.price / 4;

                    ImGui::BeginChildFrame(ImGui::GetID(("RestoreFrame" + std::to_string(i)).c_str()), ImVec2(0, 350), ImGuiWindowFlags_NoScrollbar);
                    ImVec2 windowSize = ImGui::GetContentRegionAvail();

                    // Resmi ortala
                    if (car.texture)
                    {
                        ImVec2 texSize(512, 256);
                        ImVec2 pos = ImVec2((windowSize.x - texSize.x) * 0.5f, 0);
                        ImGui::SetCursorPos(pos);
                        ImGui::Image(car.texture, texSize);


                        // Resim için border
                        ImVec2 pMin = ImGui::GetItemRectMin();
                        ImVec2 pMax = ImGui::GetItemRectMax();
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 255), 8.0f, 0, 2.0f);
                    }

                    // Araç ismini ortala
					const char* presetxname = car.presetName.c_str();
                    std::string carName = car.displayName;
                    ImVec2 textSize = ImGui::CalcTextSize(carName.c_str());
                    ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);
                    ImGui::Text("%s", carName.c_str());

                    // Restore butonu veya tamamlandı yazısı
                    ImVec2 buttonSize(120, 25);
                    ImGui::SetCursorPosX((windowSize.x - buttonSize.x) * 0.5f);

                    if (car.restored)
                    {
                        ImGui::BeginDisabled();
                        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), u8"Bu Aracın Restorasyonu Tamamlandı!");
                        ImGui::EndDisabled();
                        if (!car.caradded)
                        {
                            NFSC::AchievementCarManager::QueueAddCarByName(car.presetName.c_str());
                            car.caradded = true; // bir daha eklenmesin
                        }
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.6f, 0.1f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.5f, 0.05f, 1.0f));

                        if (ImGui::Button("Restore Et", buttonSize))
                        {
                            if (cash >= restoreCost)
                            {
                                DALCareer::SetCash(cash - restoreCost);
                                car.restored = true;
                                SaveCarPurchases();
                            }
                            else
                                SetNotify(u8"Yetersiz Para", u8"Restore için paranız yetersiz!", nullptr);
                        }

                        ImGui::PopStyleColor(3);
                    }

                    ImGui::EndChildFrame();
                }

                ImGui::PopID();
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void RenderMenu(LPDIRECT3DDEVICE9 pDevice)
{
    ImGuiIO& io = ImGui::GetIO();

    // Menü toggle tuşu (Numpad1)
    if (GetAsyncKeyState(VK_NUMPAD1) & 1)
        show_menu = !show_menu;

    if (GetAsyncKeyState('B') & 1)
    {
        cFEng* feng = cFEng::Instance();
        if (!feng) return;
        if (!feng->IsPackagePushed("FeCarSelect.fng")) return;
        g_ShowCarShop = !g_ShowCarShop;
    }

    // Menü veya popup açık mı?
    bool anyUIOpen = show_menu || g_ShowCarShop;

    // ImGui input ayarları
    io.MouseDrawCursor = anyUIOpen;
    io.WantCaptureMouse = anyUIOpen;
    io.WantCaptureKeyboard = anyUIOpen;

    // Popup modal açıksa sadece popup render et

    // Menü açık ise render et
    if (show_menu)
    {
        ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("NFSPGP Paneli", &show_menu))
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "NFSPGP Hileler");
            ImGui::Separator();

            if (ImGui::Checkbox(u8"Sınırsız NOS", &infinite_nos)) SetInfiniteNos(infinite_nos);
            if (ImGui::Checkbox("Hayalet Araba", &ghost_car)) SetGhostCar(ghost_car);

            static char g_CarInput[64] = "";
            ImGui::InputText("Preset XName", g_CarInput, IM_ARRAYSIZE(g_CarInput));
            if (ImGui::Button("Araba Ekle"))
            {
                if (!NFSC::AchievementCarManager::HasCarDB())
                {
                    SetNotify(u8"Garaj Verisi Yok!", u8"Aracınızın garaja eklenebilmesi için garaja gidin!", nullptr);
                    return;
                }
                if (g_CarInput[0] == '\0')
                {
                    SetNotify("HATA", u8"Lütfen bir preset adı girin!", nullptr);
                    return;
                }
                NFSC::AchievementCarManager::QueueAddCarByName(g_CarInput);
                SetNotify("DEBUG", u8"Araç garaja eklendi!", nullptr);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // --- YARIŞ ZİNCİRİ GRID ---
            DrawRaceChainGridUI();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // --- BAŞARIMLAR ---
            ImGui::TextColored(ImVec4(0.03f, 0.94f, 0.84f, 1.0f), u8"Kariyer Başarımları");
            ImGui::BeginChild("AchievementScroll", ImVec2(0, 260), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

            for (size_t i = 0; i < myAchievements.size(); i++) {
                ImGui::PushID((int)i);

                if (!myAchievements[i].unlocked)
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);

                ImGui::BeginGroup();
                if (myAchievements[i].texture)
                    ImGui::Image((void*)myAchievements[i].texture, ImVec2(50, 50));
                else
                    ImGui::Button("??", ImVec2(50, 50));

                ImGui::SameLine();

                ImGui::BeginGroup();
                ImGui::TextColored(ImVec4(1, 1, 1, 1), myAchievements[i].name.c_str());

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                ImGui::TextWrapped(myAchievements[i].description.c_str());
                ImGui::PopStyleColor();

                if (myAchievements[i].unlocked)
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), u8"[TAMAMLANDI]");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), u8"[KİLİTLİ]");

                ImGui::EndGroup();
                ImGui::EndGroup();

                if (!myAchievements[i].unlocked)
                    ImGui::PopStyleVar();

                ImGui::Separator();
                ImGui::Spacing();
                ImGui::PopID();
            }

            ImGui::EndChild();

            ImGui::Spacing();
            if (ImGui::Button("Kapat", ImVec2(120, 35)))
                show_menu = false;
        }
        ImGui::End();
    }
}