#include "imgui-1.92.5/imgui.h"
#include "imgui-1.92.5/backends/imgui_impl_dx9.h"
#include "imgui-1.92.5/backends/imgui_impl_win32.h"
#include "Patches.h" // Kendi hile fonksiyonlarýmýz için
#include "Console.h"   
#include "d3d9.h"
#include "C:\Program Files (x86)\directxsdk\Include\d3dx9.h"
#include <string>
#include <vector>
#include <windows.h>
#include <fstream>
#include "mmsystem.h"
#pragma comment(lib, "C:/Program Files (x86)/directxsdk/Lib/x86/d3dx9.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "winmm.lib")
extern std::vector<Achievement> myAchievements;

extern LPDIRECT3DDEVICE9 g_pd3dDevice;
bool millionaireNotified = false;
static int eskiPara = -1;

std::vector<Achievement> myAchievements = {
    {(const char*)u8"ÝLK ADIM", (const char*)u8"11.200$ nakit paraya ulaþtýn.", "GLOBAL\\achievement\\money.png", false, nullptr},
    {(const char*)u8"HIZ TUTKUNU", (const char*)u8"300 KM/H hýzý geçtin.", "GLOBAL\\achievement\\speed.png", false, nullptr},
    {(const char*)u8"KANUN KAÇAÐI", (const char*)u8"50 polisi pert ettin.", "GLOBAL\\achievement\\cop.png", false, nullptr}
};

void PlayAchievementSound() {
    // Önce varsa açýk olan sesi kapat (üst üste binmesin diye)
    mciSendStringA("close achievement_sound", NULL, 0, NULL);

    // MP3 dosyasýný aç (Dosya yolunun doðru olduðundan emin ol)
    // Örn: "GLOBAL\\ASSETS\\tink.mp3"
    mciSendStringA("open \"GLOBAL\\achievement\\completed.mp3\" type mpegvideo alias achievement_sound", NULL, 0, NULL);

    // Sesi çal
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


void RenderNotification() {
    if (!g_NotifyShow) return;

    ImGuiIO& io = ImGui::GetIO();
    g_NotifyTimer += io.DeltaTime;

    float x_pos = -400.0f; // Baþlangýç pozisyonu (Ekran dýþý sol)
    float total_time = 4.0f; // Toplam görünme süresi
    float anim_speed = 0.6f; // Giriþ ve çýkýþ hýzý

    // --- Pepega Tarzý Kayma Animasyonu ---
    if (g_NotifyTimer < anim_speed) { // GÝRÝÞ (Soldan içeri kayar)
        float t = g_NotifyTimer / anim_speed;
        // Yumuþak geçiþ (Smoothstep) eklemek istersen: t = t * t * (3 - 2 * t);
        x_pos = -400.0f + (t * 420.0f);
    }
    else if (g_NotifyTimer < (total_time - anim_speed)) { // BEKLEME (Ekranda sabit)
        x_pos = 20.0f;
    }
    else if (g_NotifyTimer < total_time) { // ÇIKIÞ (Geriye doðru kayar)
        float t = (g_NotifyTimer - (total_time - anim_speed)) / anim_speed;
        x_pos = 20.0f - (t * 420.0f);
    }
    else { // BÝTÝÞ
        g_NotifyShow = false;
        return;
    }

    // --- Pencere Ayarlarý ---
    ImGui::SetNextWindowPos(ImVec2(x_pos, 50.0f));
    ImGui::SetNextWindowSize(ImVec2(380, 0)); // Geniþlik resim için ideal

    // Pencere Stili (Saydamlýk ve Arkaplan)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 0.94f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.03f, 0.94f, 0.84f, 0.6f)); // Turkuaz çerçeve

    ImGui::Begin("##GlobalNotify", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav);

    // --- ÝÇERÝK: RESÝM + METÝN ---
    if (g_CurrentAchievementTexture != nullptr) {
        // Ýkonu 55x55 boyutunda çiz
        ImGui::Image((void*)g_CurrentAchievementTexture, ImVec2(55, 55));

        // Yanýna geç
        ImGui::SameLine();
    }

    // Metin bloðunu baþlat
    ImGui::BeginGroup();
    // Baþlýk (Turkuaz)
    ImGui::TextColored(ImVec4(0.03f, 0.94f, 0.84f, 1.0f), g_NotifyTitle);

    ImGui::Separator();

    // Mesaj (Beyaz ve Alt Satýra Geçebilir)
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 280.0f);
    ImGui::Text(g_NotifyMsg);
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();

    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void TriggerAchievement(const char* title, const char* message, const char* imagePath) {
    // 1. Cihaz kontrolü (Global g_pd3dDevice kullanýyoruz)
    PlayAchievementSound();
    if (g_NotifyShow || !g_pd3dDevice) return;

    // 2. Eski resmi temizle (Bellek sýzýntýsýný önlemek için önemli)
    if (g_CurrentAchievementTexture != nullptr) {
        g_CurrentAchievementTexture->Release();
        g_CurrentAchievementTexture = nullptr;
    }

    // 3. Yeni resmi yükle
    HRESULT hr = D3DXCreateTextureFromFileA(g_pd3dDevice, imagePath, &g_CurrentAchievementTexture);
    if (FAILED(hr)) {
        g_CurrentAchievementTexture = nullptr;
    }

    // 4. Global metin deðiþkenlerini doldur
    strncpy_s(g_NotifyTitle, title, sizeof(g_NotifyTitle));
    strncpy_s(g_NotifyMsg, message, sizeof(g_NotifyMsg));

    // 5. Gösterimi baþlat
    g_NotifyTimer = 0.0f;
    g_NotifyShow = true;
}

void SaveAchievementsSecure() {
    std::ofstream file("GLOBAL\\stats.dat", std::ios::binary);
    if (!file.is_open()) return;

    // 1. Önce kaç tane baþarým olduðunu yaz (Dosya yapýsýný korumak için)
    size_t count = myAchievements.size();
    file.write((char*)&count, sizeof(count));

    // 2. Her birinin unlocked durumunu yaz
    for (const auto& ach : myAchievements) {
        file.write((char*)&ach.unlocked, sizeof(bool));
    }

    // 3. Dosyanýn sonuna basit bir "Anahtar" ekle (Dosya bütünlüðü için)
    int magicKey = 0xDEADC0DE;
    file.write((char*)&magicKey, sizeof(magicKey));

    file.close();
}

void LoadAchievementsSecure() {
    std::ifstream file("GLOBAL\\stats.dat", std::ios::binary);
    if (!file.is_open()) return;

    size_t savedCount;
    file.read((char*)&savedCount, sizeof(savedCount));

    // Eðer baþarým sayýsý uyuþmuyorsa dosya kurcalanmýþ olabilir
    if (savedCount != myAchievements.size()) {
        file.close();
        return;
    }

    // unlocked durumlarýný geri yükle
    for (auto& ach : myAchievements) {
        file.read((char*)&ach.unlocked, sizeof(bool));
    }

    // Dosya sonundaki anahtarý kontrol et
    int readKey;
    file.read((char*)&readKey, sizeof(readKey));

    if (readKey != 0xDEADC0DE) {
		MessageBoxA(NULL, "Baþarým dosyayýnda deðiþiklik yapýldýðý tespit edildi. Tüm baþarýmlar sýfýrlandý.", "Hata", MB_OK | MB_ICONERROR);
        for (auto& ach : myAchievements) ach.unlocked = false;
    }

    file.close();
}

void AchievementUpdate() {
    // IsBadReadPtr kullanýmý eski olsa da bu tarz projelerde hayat kurtarýr. 
    // Ama daha basitçe adresin kendisini kontrol edelim:
    uintptr_t cashBase = 0x0516AD40;

    // Carbon v1.4 için bu adresin iþaret ettiði yerin geçerli olup olmadýðýna bakýyoruz
    __try {
        int* cashAddr = (int*)cashBase;
        if (!cashAddr || (uintptr_t)cashAddr < 0x10000) return; // Geçersiz bellek bölgesi

        int suAnkiPara = *cashAddr;

        // --- BAÞARIM 0: ÝLK ADIM ---
        if (suAnkiPara >= 500000 && !myAchievements[0].unlocked) {
            myAchievements[0].unlocked = true;
            TriggerAchievement(
                myAchievements[0].name.c_str(),
                myAchievements[0].description.c_str(),
                myAchievements[0].iconPath
            );
            SaveAchievementsSecure();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Eðer oyun henüz adresi oluþturmadýysa buraya düþer ve çökmez
        return;
    }
}

// Parametre olarak LPDIRECT3DDEVICE9 ekledikhal
void RenderMenu(LPDIRECT3DDEVICE9 pDevice) {
    if (GetAsyncKeyState(VK_NUMPAD1) & 1) show_menu = !show_menu;
    ImGuiIO& io = ImGui::GetIO();

    if (show_menu) {
        io.MouseDrawCursor = true;
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        *(BYTE*)0x00A712B0 = 0;
    }
    else {
        io.MouseDrawCursor = false;
        *(BYTE*)0x00A712B0 = 1;
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("NFSPGP Paneli", &show_menu)) {

        // --- HÝLELER ---
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "NFSPGP Hileler");
        ImGui::Separator();

        if (ImGui::Checkbox("Sýnýrsýz NOS", &infinite_nos)) SetInfiniteNos(infinite_nos);
        if (ImGui::Checkbox("Hayalet Araba", &ghost_car)) SetGhostCar(ghost_car);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- BAÞARIMLAR GALERÝSÝ ---
        ImGui::TextColored(ImVec4(0.03f, 0.94f, 0.84f, 1.0f), "Kariyer Baþarýmlarý");

        // Slider Alaný (BeginChild)
        ImGui::BeginChild("AchievementScroll", ImVec2(0, 320), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        for (size_t i = 0; i < myAchievements.size(); i++) {
            ImGui::PushID(i);

            // Kilitli baþarýmlarý %40 görünürlükle göster
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
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[TAMAMLANDI]");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[KÝLÝTLÝ]");
            ImGui::EndGroup();
            ImGui::EndGroup();

            if (!myAchievements[i].unlocked) ImGui::PopStyleVar();

            ImGui::Separator();
            ImGui::Spacing();
            ImGui::PopID();
        }

        ImGui::EndChild();

        ImGui::Spacing();
        if (ImGui::Button("Kapat", ImVec2(120, 35))) show_menu = false;
    }
    ImGui::End();

    RenderNotification();
}
