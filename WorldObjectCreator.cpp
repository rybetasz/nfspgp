#include "WorldObjectCreator.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include <d3d9.h>
#include <d3dx9.h>

// ============================================================
// GameFlow
// ============================================================
//
// 0x00A99BBC doğrudan GameFlow integer değerini tutuyor.
//
// 6 = Race / World
//
// ============================================================

#define THEGAMEFLOWMANAGER_ADDRESS 0x00A99BBC


// ============================================================
// bMatrix4
// ============================================================

struct bMatrix4
{
    float m[4][4];
};


// ============================================================
// WorldModel
// ============================================================
//
// Gerçek class layout'u burada gerekli değil.
// Sadece pointer olarak kullanıyoruz.
//

class WorldModel
{};


// ============================================================
// Function types
// ============================================================

// ============================================================
// bOMalloc
// ============================================================
//
// 0x00477BE0
//
// Carbon:
//
// bOMalloc(SlotPool* pool)
//
// Assembly:
//
// mov ecx, [esp+4]
// jmp SlotPool::Malloc
// retn
//
// Caller stack'i kendisi temizlediği için __cdecl.
//

using bOMalloc_t =
void* (__cdecl*)(
    void* slotPool
    );


// ============================================================
// WorldModel Constructor
// ============================================================
//
// 0x007E1C40
//
// WorldModel::WorldModel(
//     uint32_t modelHash,
//     bMatrix4* matrix,
//     bool,
//     bool
// )
//
// ECX = object
//

using WorldModelCtor_t =
WorldModel * (__thiscall*)(
    WorldModel*,
    uint32_t,
    bMatrix4*,
    bool,
    bool
    );


// ============================================================
// WorldModel Deleting Destructor
// ============================================================
//
// 0x007D7720
//
// ECX = WorldModel*
//
// arg = 1:
//
//     WorldModel destructor
//     +
//     bClose(WorldModelSlotPool, this)
//
// Bu yüzden tamamen yok etmek için bunu kullanıyoruz.
//

using WorldModelDelete_t =
WorldModel * (__thiscall*)(
    WorldModel*,
    int
    );


// ============================================================
// Game addresses
// ============================================================

static constexpr uintptr_t ADDR_bOMalloc =
0x00477BE0;

static constexpr uintptr_t ADDR_WorldModelCtor =
0x007E1C40;

static constexpr uintptr_t ADDR_WorldModelDelete =
0x007D7720;

static constexpr uintptr_t ADDR_WorldModelSlotPool =
0x00B74D1C;


// ============================================================
// Test model
// ============================================================

static WorldModel* g_TestWorldModel = nullptr;


// ============================================================
// Test model hash
// ============================================================
//
// Şu an kullandığın model:
//
// 0x27498C46
//
// İstersen burayı değiştirerek başka model kullanabilirsin.
//

static constexpr uint32_t TEST_WORLD_MODEL_HASH =
0x27498C46;


// ============================================================
// HOT file
// ============================================================

static constexpr const char* TEST_HOT_FILE =
"TRACKS\\HotPositionL5RA.HOT";


// ============================================================
// HOT Position Reader
// ============================================================
//
// HOT dosyasından yalnızca:
//
// X
// Y
// Z
//
// okunuyor.
//
// Angle ve Speed tamamen ignore ediliyor.
//

bool ReadHotPosition(
    const char* filePath,
    int index,
    HotPosition& outPosition)
{
    if (!filePath)
        return false;

    if (index < 0)
        return false;

    FILE* file = nullptr;

    if (fopen_s(&file, filePath, "r") != 0 || !file)
    {
        OutputDebugStringA(
            "[NFSPGP] Failed to open HOT file.\n"
        );

        return false;
    }

    char line[512];

    int currentIndex = 0;

    while (fgets(line, sizeof(line), file))
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        int result =
            sscanf_s(
                line,
                "HOTPOSITION: %f,%f,%f",
                &x,
                &y,
                &z
            );

        if (result == 3)
        {
            if (currentIndex == index)
            {
                outPosition.x = x;
                outPosition.y = y;
                outPosition.z = z;

                fclose(file);

                return true;
            }

            ++currentIndex;
        }
    }

    fclose(file);

    return false;
}


// ============================================================
// Destroy WorldModel
// ============================================================
//
// ÖNEMLİ:
//
// g_TestWorldModel'i destructor çağrısından ÖNCE NULL yapıyoruz.
//
// Böylece destructor sırasında herhangi bir callback / update
// GetTestWorldModel() çağırırsa artık geçersiz pointer dönmez.
//
// 0x007D7720 kullanılıyor.
//
// 0x007D1950 destructor body'dir.
// 0x007D7720 ise deleting destructor'dır.
//
// arg = 1 -> bClose(WorldModelSlotPool, this)
//

void DestroyTestWorldModel()
{
    WorldModel* model =
        g_TestWorldModel;

    if (!model)
        return;

    // --------------------------------------------------------
    // Önce pointer'ı temizle.
    // --------------------------------------------------------

    g_TestWorldModel = nullptr;

    OutputDebugStringA(
        "[NFSPGP] Destroying test WorldModel...\n"
    );

    // --------------------------------------------------------
    // Deleting destructor
    // --------------------------------------------------------

    auto WorldModelDelete =
        reinterpret_cast<WorldModelDelete_t>(
            ADDR_WorldModelDelete
            );

    // --------------------------------------------------------
    // arg = 1
    //
    // Destructor +
    // WorldModelSlotPool üzerinden bClose
    // --------------------------------------------------------

    WorldModelDelete(
        model,
        1
    );

    OutputDebugStringA(
        "[NFSPGP] Test WorldModel destroyed.\n"
    );
}


// ============================================================
// Spawn WorldModel
// ============================================================

WorldModel* SpawnWorldObject(
    uint32_t hash,
    float x,
    float y,
    float z,
    float rotX,
    float rotY,
    float rotZ)
{
    // ========================================================
    // Function pointers
    // ========================================================

    auto bOMalloc =
        reinterpret_cast<bOMalloc_t>(
            ADDR_bOMalloc
            );

    auto WorldModelCtor =
        reinterpret_cast<WorldModelCtor_t>(
            ADDR_WorldModelCtor
            );


    // ========================================================
    // WorldModelSlotPool
    // ========================================================
    //
    // 0x00B74D1C = WorldModelSlotPool global pointer
    //
    // Yani:
    //
    // *(void**)0x00B74D1C
    //
    // gerçek SlotPool adresidir.
    //

    void* worldModelPool =
        *reinterpret_cast<void**>(
            ADDR_WorldModelSlotPool
            );

    if (!worldModelPool)
    {
        OutputDebugStringA(
            "[NFSPGP] WorldModelSlotPool == NULL!\n"
        );

        return nullptr;
    }


    // ========================================================
    // Matrix
    // ========================================================

    bMatrix4 matrix{};


    // ========================================================
    // Rotation
    // ========================================================
    //
    // rotY = Yaw
    // rotX = Pitch
    // rotZ = Roll
    //
    // Input değerleri degree.
    //

    D3DXMATRIX rotation;

    D3DXMatrixRotationYawPitchRoll(
        &rotation,

        D3DXToRadian(rotY),
        D3DXToRadian(rotX),
        D3DXToRadian(rotZ)
    );


    // ========================================================
    // D3DXMATRIX -> bMatrix4
    // ========================================================

    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            matrix.m[row][col] =
                rotation.m[row][col];
        }
    }


    // ========================================================
    // Position
    // ========================================================
    //
    // Carbon bMatrix4:
    //
    // +30 = X
    // +34 = Y
    // +38 = Z
    //
    // 4x4 float matrix olduğu için:
    //
    // m[3][0] = +30
    // m[3][1] = +34
    // m[3][2] = +38
    //

    matrix.m[3][0] = x;
    matrix.m[3][1] = y;
    matrix.m[3][2] = z;


    // ========================================================
    // Allocate WorldModel
    // ========================================================
    //
    // bOMalloc:
    //
    // bOMalloc(WorldModelSlotPool)
    //
    // sonucunda WorldModel için 0xA0 byte slot geliyor.
    //

    WorldModel* obj =
        static_cast<WorldModel*>(
            bOMalloc(
                worldModelPool
            )
            );

    if (!obj)
    {
        OutputDebugStringA(
            "[NFSPGP] WorldModel bOMalloc failed!\n"
        );

        return nullptr;
    }


    // ========================================================
    // Constructor
    // ========================================================

    WorldModel* result =
        WorldModelCtor(
            obj,
            hash,
            &matrix,
            false,
            false
        );

    if (!result)
    {
        OutputDebugStringA(
            "[NFSPGP] WorldModel constructor failed!\n"
        );

        return nullptr;
    }


    // ========================================================
    // Global pointer
    // ========================================================

    g_TestWorldModel =
        result;


    // ========================================================
    // Debug information
    // ========================================================

    char buffer[512];

    sprintf_s(
        buffer,
        sizeof(buffer),

        "[NFSPGP] WorldModel spawned\n"
        "Object: %p\n"
        "Hash: %08X\n"
        "Position: %.2f %.2f %.2f\n"
        "Rotation: %.2f %.2f %.2f\n",

        result,

        hash,

        x,
        y,
        z,

        rotX,
        rotY,
        rotZ
    );

    OutputDebugStringA(buffer);


    // ========================================================
    // Return
    // ========================================================

    return result;
}


// ============================================================
// Spawn from HOT
// ============================================================
//
// hotPositionIndex:
//     HOT içindeki 0-based position index.
//
// Rotation:
//     Manuel olarak degree.
//
// ============================================================

bool SpawnWorldModelFromHotPosition(
    uint32_t hash,
    int hotPositionIndex,
    float rotX,
    float rotY,
    float rotZ)
{
    // ========================================================
    // Zaten object varsa tekrar spawn yapma
    // ========================================================

    if (g_TestWorldModel)
    {
        return true;
    }


    // ========================================================
    // Read HOT
    // ========================================================

    HotPosition position{};

    if (!ReadHotPosition(
        TEST_HOT_FILE,
        hotPositionIndex,
        position))
    {
        OutputDebugStringA(
            "[NFSPGP] Failed to read HOT position!\n"
        );

        return false;
    }


    // ========================================================
    // Spawn
    // ========================================================

    WorldModel* obj =
        SpawnWorldObject(
            hash,

            position.x,
            position.y,
            position.z,

            rotX,
            rotY,
            rotZ
        );

    if (!obj)
        return false;


    return true;
}


// ============================================================
// Get test WorldModel
// ============================================================

WorldModel* GetTestWorldModel()
{
    return g_TestWorldModel;
}


// ============================================================
// GameFlow Monitor
// ============================================================
//
// GameFlow:
//
// 6 = Race / World
//
// Davranış:
//
// 5 -> 6
//     WorldModel spawn edilir.
//
// 6 -> 6
//     Hiçbir şey yapılmaz.
//
// 6 -> 5
// 6 -> 4
// 6 -> 0
// 6 -> 7
//     WorldModel destroy edilir.
//
// ============================================================

void UpdateWorldModelGameFlow()
{
    static int previousGameFlow = -1;


    // ========================================================
    // GameFlow oku
    // ========================================================
    //
    // 0x00A99BBC doğrudan integer.
    //

    int gameFlow =
        *reinterpret_cast<int*>(
            THEGAMEFLOWMANAGER_ADDRESS
            );


    // ========================================================
    // WORLD / RACE
    // ========================================================
    //
    // Sadece GameFlow 6'da spawn etmeye izin ver.
    //
    // g_TestWorldModel NULL ise spawn yapılır.
    //
    // Böylece her 10 ms'de tekrar constructor çağırılmaz.
    //

    if (gameFlow == 6)
    {
        if (!g_TestWorldModel)
        {
            // ------------------------------------------------
            // Test model
            // ------------------------------------------------
            //
            // Hash:
            //     0x27498C46
            //
            // HOT index:
            //     0
            //
            // Rotation:
            //     X = 0
            //     Y = 0
            //     Z = 135.5
            //

            SpawnWorldModelFromHotPosition(
                TEST_WORLD_MODEL_HASH,
                0,

                0.0f,     // rotX
                0.0f,     // rotY
                135.5f    // rotZ
            );
        }
    }


    // ========================================================
    // GameFlow değişmediyse başka işlem yok
    // ========================================================

    if (gameFlow == previousGameFlow)
    {
        return;
    }


    // ========================================================
    // 6 -> başka state
    // ========================================================

    if (previousGameFlow == 6 &&
        gameFlow != 6)
    {
        OutputDebugStringA(
            "[NFSPGP] GameFlow changed: 6 -> non-6\n"
            "[NFSPGP] Removing test WorldModel...\n"
        );

        DestroyTestWorldModel();
    }


    // ========================================================
    // İlk state / yeni state
    // ========================================================

    previousGameFlow =
        gameFlow;
}