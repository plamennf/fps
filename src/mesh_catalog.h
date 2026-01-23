#pragma once

#include "hash_table.h"

struct Mesh;

enum Mesh_Load_State {
    MESH_STATE_LOADING,
    MESH_STATE_CPU_READY,
    MESH_STATE_READY,
    MESH_STATE_FAILED,
};

struct Mesh_Entry {
    Mesh *mesh;
    Mesh_Load_State state;
    char filepath[1024];
};

struct Mesh_Catalog {
    String_Hash_Table <Mesh_Entry> mesh_lookup;
    
    Mesh *find_or_load(char *name);
    void update();
};
