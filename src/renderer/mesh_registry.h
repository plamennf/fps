#pragma once

struct Mesh;

struct Mesh_Registry {
    eastl::unordered_map <eastl::string, Mesh *> mesh_lookup;
    eastl::vector <char *> all_names_in_order_of_loading;
    
    ~Mesh_Registry();
    
    Mesh *find_or_load(char *name);

    void recursive_init_all();
};
