#pragma once

#include "renderer/scene_renderer.h"
#include "renderer/mesh.h"

enum Terrain_Object_Type {
    TERRAIN_OBJECT_TREE,
    TERRAIN_OBJECT_ROCK,
    TERRAIN_OBJECT_PEBBLE_SQUARE,
    TERRAIN_OBJECT_PEBBLE_ROUND,
    TERRAIN_OBJECT_MUSHROOM,
    TERRAIN_OBJECT_GRASS,
    TERRAIN_OBJECT_FERN,
    TERRAIN_OBJECT_BUSH,
    NUM_TERRAIN_OBJECT_TYPES,
};

struct Terrain_Object_Instance_Data {
    Terrain_Object_Type type;
    glm::vec3 position;
    float scale;
    float rotation;
    Mesh *mesh;
    int variation;
};

struct Terrain_Chunk {
    int num_vertices_per_side;
    float scale;
    glm::vec3 offset;
    
    Submesh submesh;
    Mesh mesh;

    eastl::vector <Terrain_Object_Instance_Data> objects;

    struct Terrain_Object_Instance_Batch {
        Terrain_Object_Type type;
        int variation;
        int start_index;
        int count;
    };

    eastl::vector <Terrain_Object_Instance_Batch> batches;
    
    void generate(int num_vertices_per_side, float scale, glm::vec3 offset, int num_objects_to_place);
    float get_height(float x, float z);
};
