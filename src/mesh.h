#pragma once

#include "geometry.h"

#define MESH_FILE_VERSION 1

struct Mesh_Vertex;
struct Texture;
struct Gpu_Buffer;

struct Material {
    char *diffuse_texture_name;
    char *specular_texture_name;
    char *normal_texture_name;
    
    Vector4 diffuse_color;
    float shininess;
};

struct Submesh {
    int num_indices;
    u32 *indices;
    
    int num_vertices;
    Mesh_Vertex *vertices;

    Gpu_Buffer *vertex_buffer;
    Gpu_Buffer *index_buffer;
    
    Material material;
};

struct Mesh {
    int num_submeshes;
    Submesh *submeshes;
};

bool load_mesh(Mesh *mesh, char *filepath);
bool save_mesh(Mesh *mesh, char *filepath);
