#pragma once

#include "geometry.h"

struct Entity_Manager;
struct Mesh;

struct Entity {
    u32 id = 0;
    Entity_Manager *manager = NULL;
    bool scheduled_for_destruction = false;

    Vector3 position = v3(0, 0, 0);
    Quaternion orientation;
    Vector3 scale = v3(1, 1, 1);
    Vector4 scale_color = v4(1, 1, 1, 1);

    char *mesh_name = NULL;
};

void set_mesh(Entity *entity, char *mesh_name);
