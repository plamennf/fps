#pragma once

#include "hash_table.h"

struct Mesh;

struct Mesh_Catalog {
    String_Hash_Table <Mesh *> mesh_lookup;

    Mesh *find_or_load(char *name);
};
