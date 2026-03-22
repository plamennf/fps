#pragma once

#include "renderer/scene_renderer.h"
#include "renderer/mesh.h"

struct Terrain_Chunk {
    int num_vertices_per_side;
    float scale;
    glm::vec3 offset;
    
    Submesh submesh;
    Mesh mesh;
    
    void generate(int num_vertices_per_side, float scale, glm::vec3 offset);
    float get_height(float x, float z);
};
