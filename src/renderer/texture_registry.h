#pragma once

struct Texture;

struct Texture_Registry {
    eastl::unordered_map <eastl::string, Texture *> texture_lookup;
    
    Texture *find_or_load(char *name);

    void recursive_init_all();
};
