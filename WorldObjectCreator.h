#pragma once
#include <cstdint>

struct HotPosition
{
    float x;
    float y;
    float z;
};

class WorldModel;

bool ReadHotPosition(const char* filePath, int index, HotPosition& outPosition);

WorldModel* SpawnWorldObject(
    uint32_t hash,
    float x,
    float y,
    float z,
    float rotX,
    float rotY,
    float rotZ
);

bool SpawnWorldModelFromHotPosition(
    uint32_t hash,
    int hotPositionIndex,
    float rotX,
    float rotY,
    float rotZ
);

WorldModel* GetTestWorldModel();

void DestroyTestWorldModel();
void UpdateWorldModelGameFlow();