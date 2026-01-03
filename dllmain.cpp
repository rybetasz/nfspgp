#include <windows.h>
#include "Patches.h"

DWORD WINAPI WorkerThread(LPVOID lpParam) {
    // Oyunun exe'sinin ve String Manager'ın bellekte açılması için süre tanı
    Sleep(5000);

    LoadConfig(); // Ayarları bir kez oku

    while (true) {
        RunPointerWatcher(); // Hafızayı sürekli denetle
        Sleep(100); // İşlemciyi yormamak için kısa bekleme
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        // DLL yüklendiğinde arka plan işçisini başlat
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThread, NULL, 0, NULL);
    }
    return TRUE;
}