#pragma once

#include "hash_table.h"
#include "draw.h"

struct Texture;

struct Texture_Cpu {
    int width;
    int height;
    
    Texture_Format format;
    int bpp;

    u8 *data;
};

enum Texture_Load_State {
    TEXTURE_STATE_LOADING,
    TEXTURE_STATE_CPU_READY,
    TEXTURE_STATE_READY,
    TEXTURE_STATE_FAILED,
};

struct Texture_Entry {
    Texture *texture;
    Texture_Cpu cpu;
    Texture_Load_State state;
    char filepath[1024];
};

struct Texture_Catalog {    
    String_Hash_Table <Texture_Entry> texture_lookup;

    Texture *find_or_load(char *name);
    void update();
};
