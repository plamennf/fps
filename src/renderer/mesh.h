#pragma once

#include "render_backend.h"

#define MESH_FILE_VERSION 1

struct Texture;
struct Gpu_Buffer;

struct Material {
    char *name = NULL;
    bool uses_specular_glossiness = false;
    
    char *albedo_texture_name = NULL;
    Texture *albedo_texture = NULL;
    glm::vec4 albedo_factor = glm::vec4(1, 1, 1, 1);
    
    char *normal_texture_name = NULL;
    Texture *normal_texture = NULL;

    char *metallic_roughness_texture_name = NULL;
    Texture *metallic_roughness_texture = NULL;

    char *ao_texture_name = NULL;
    Texture *ao_texture = NULL;

    char *emissive_texture_name = NULL;
    Texture *emissive_texture = NULL;
    glm::vec3 emissive_factor = glm::vec3(0, 0, 0);

    VkDescriptorSet descriptor_set;
    Gpu_Buffer uniform_buffer;
};

struct Submesh {
    int num_indices;
    u32 *indices;
    
    int num_vertices;
    Mesh_Vertex *vertices;

    bool has_gpu_data;
    
    Gpu_Buffer vertex_buffer;
    Gpu_Buffer index_buffer;
    
    Material material;
};

struct Mesh {
    const char *filename;
    
    int num_submeshes;
    Submesh *submeshes;
};

bool load_mesh(Mesh *mesh, char *filepath);
bool save_mesh(Mesh *mesh, char *filepath);
