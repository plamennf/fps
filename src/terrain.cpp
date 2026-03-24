#include "pch.h"
#include "terrain.h"
#include "renderer/mesh_registry.h"
#include "main.h"

static int num_variations_per_terrain_object_type[NUM_TERRAIN_OBJECT_TYPES] = {
    5, // TERRAIN_OBJECT_TREE
    3, // TERRAIN_OBJECT_ROCK
    6, // TERRAIN_OBJECT_PEBBLE_SQUARE
    5, // TERRAIN_OBJECT_PEBBLE_ROUND
    2, // TERRAIN_OBJECT_MUSHROOM
    2, // TERRAIN_OBJECT_GRASS
    1, // TERRAIN_OBJECT_FERN
    2, // TERRAIN_OBJECT_BUSH
};

static Mesh *get_mesh_for_terrain_object_type(Terrain_Object_Type type, int variation) {
    char name[128];
    
    switch (type) {
        case TERRAIN_OBJECT_TREE: {
            snprintf(name, sizeof(name), "BirchTree_%d", variation + 1);
        } break;

        case TERRAIN_OBJECT_ROCK: {
            snprintf(name, sizeof(name), "Rock_Medium_%d", variation + 1);
        } break;

        case TERRAIN_OBJECT_PEBBLE_SQUARE: {
            snprintf(name, sizeof(name), "Pebble_Square_%d", variation + 1);
        } break;

        case TERRAIN_OBJECT_PEBBLE_ROUND: {
            snprintf(name, sizeof(name), "Pebble_Round_%d", variation + 1);
        } break;

        case TERRAIN_OBJECT_MUSHROOM: {
            if (variation == 0) {
                snprintf(name, sizeof(name), "Mushroom_Common");
            } else if (variation == 1) {
                snprintf(name, sizeof(name), "Mushroom_Laetiporus");
            }
        } break;

        case TERRAIN_OBJECT_GRASS: {
            if (variation == 0) {
                snprintf(name, sizeof(name), "Grass_Common_Short");
            } else if (variation == 1) {
                snprintf(name, sizeof(name), "Grass_Common_Tall");
            }
        } break;

        case TERRAIN_OBJECT_FERN: {
            snprintf(name, sizeof(name), "Fern_1");
        } break;

        case TERRAIN_OBJECT_BUSH: {
            if (variation == 0) {
                snprintf(name, sizeof(name), "Bush_Common");
            } else if (variation == 1) {
                snprintf(name, sizeof(name), "Bush_Common_Flowers");
            }
        } break;

        default: {
            snprintf(name, sizeof(name), "TwistedTree_1");
        } break;
    }

    return globals.mesh_registry->find_or_load(name);
}

static int hash(int x, int z) {
    int h = x * 374761393 + z * 668265263;
    h = (h ^ (h >> 13)) * 1274126177;
    return h;
}

static float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

static float grad(int hash, float x, float z) {
    switch (hash & 3) {
        case 0: return  x + z;
        case 1: return -x + z;
        case 2: return  x - z;
        default: return -x - z;
    }
}

float noise(float x, float z) {
    int X = int(floor(x)) & 255;
    int Z = int(floor(z)) & 255;

    float xf = x - floor(x);
    float zf = z - floor(z);

    float u = fade(xf);
    float v = fade(zf);

    int h00 = hash(X,   Z  );
    int h01 = hash(X,   Z+1);
    int h10 = hash(X+1, Z  );
    int h11 = hash(X+1, Z+1);

    float g00 = grad(h00, xf,     zf);
    float g10 = grad(h10, xf - 1, zf);
    float g01 = grad(h01, xf,     zf - 1);
    float g11 = grad(h11, xf - 1, zf - 1);

    float x1 = lerp(g00, g10, u);
    float x2 = lerp(g01, g11, u);

    float result = lerp(x1, x2, v);

    return (result + 1.0f) * 0.5f;
}

float Terrain_Chunk::get_height(float x, float z) {
    float height = 0.0f;
    float amplitude = 20.0f;
    float frequency = 0.01f;
    float persistence = 0.5f;
    int octaves = 4;

    for (int i = 0; i < octaves; i++) {
        float seed_offset_x = seed * 10.0f;
        float seed_offset_z = seed * 20.0f;
        
        height += glm::perlin(glm::vec2(x + seed_offset_x, z + seed_offset_z) * frequency) * amplitude;
        frequency *= 2.0f;
        amplitude *= persistence;
    }

    //height = glm::pow(height, 1.5f);
    
    return height;
}

bool Terrain_Chunk::generate(u32 _seed, int _num_vertices_per_side, float _scale, glm::vec3 _offset, int num_objects_to_place) {
    seed                  = _seed;
    num_vertices_per_side = _num_vertices_per_side;
    scale                 = _scale;
    offset                = _offset;

    srand(seed);
    
    eastl::vector <Mesh_Vertex> vertices;
    eastl::vector <u32> indices;
    
    //
    // Generate vertices
    //
    {
        vertices.resize(num_vertices_per_side * num_vertices_per_side);
        for (int z = 0; z < num_vertices_per_side; z++) {
            for (int x = 0; x < num_vertices_per_side; x++) {
                int index = z * num_vertices_per_side + x;

                float world_x = x * scale + offset.x;
                float world_z = z * scale + offset.z;

                float height  = get_height(world_x, world_z);

                Mesh_Vertex v = {};

                v.position = glm::vec3(world_x, height, world_z);
                v.uv       = glm::vec2((float)x / num_vertices_per_side, (float)z / num_vertices_per_side);
                v.color    = glm::vec4(1, 1, 1, 1);

                vertices[index] = v;
            }
        }
    }

    //
    // Generate indices
    //
    {
        for (int z = 0; z < num_vertices_per_side - 1; z++) {
            for (int x = 0; x < num_vertices_per_side - 1; x++) {
                u32 i0 = z * num_vertices_per_side + x;
                u32 i1 = i0 + 1;
                u32 i2 = i0 + num_vertices_per_side;
                u32 i3 = i2 + 1;

                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);

                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }
    }

    //
    // Compute normals
    //
    {
        for (size_t i = 0; i < indices.size(); i += 3) {
            u32 i0 = indices[i + 0];
            u32 i1 = indices[i + 1];
            u32 i2 = indices[i + 2];

            glm::vec3 v0 = vertices[i0].position;
            glm::vec3 v1 = vertices[i1].position;
            glm::vec3 v2 = vertices[i2].position;

            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;

            glm::vec3 normal = glm::normalize_or_zero(glm::cross(edge1, edge2));

            vertices[i0].normal = normal;
            vertices[i1].normal = normal;
            vertices[i2].normal = normal;
        }
    }

    memset(&submesh, 0, sizeof(submesh));
    
    submesh.num_indices  = (int)indices.size();
    submesh.indices      = copy_to_array(indices);

    submesh.num_vertices = (int)vertices.size();
    submesh.vertices     = copy_to_array(vertices);

    submesh.material.albedo_factor = glm::vec4(1);
    
    mesh.num_submeshes = 1;
    mesh.submeshes     = &submesh;

    //
    // Generate objects
    //
    float chunk_world_size = (num_vertices_per_side - 1) * scale;
    objects.reserve(num_objects_to_place);
    for (int i = 0; i < num_objects_to_place; i++) {
        float local_x = random_float(0.0f, chunk_world_size);
        float local_z = random_float(0.0f, chunk_world_size);

        float world_x = offset.x + local_x;
        float world_z = offset.z + local_z;

        float world_y = get_height(world_x, world_z);

        glm::vec3 position = glm::vec3(world_x, world_y, world_z);

        float macro   = noise(world_x * 0.01f, world_z * 0.01f);
        float detail  = noise(world_x * 0.1f,  world_z * 0.1f);

        float density = macro * 0.8f + detail * 0.2f;

        float height_left  = get_height(world_x - 0.5f, world_z);
        float height_right = get_height(world_x + 0.5f, world_z);
        float height_up    = get_height(world_x, world_z + 0.5f);
        float height_down  = get_height(world_x, world_z - 0.5f);

        glm::vec3 normal = glm::normalize_or_zero(glm::vec3(height_left - height_right, 2.0f, height_down - height_up));
        float slope = 1.0f - normal.y;

        if (slope > 0.6f) continue;

        Terrain_Object_Type type;

        if (density > 0.7f) {
            type = TERRAIN_OBJECT_TREE;
        } else if (density > 0.55f) {
            type = TERRAIN_OBJECT_BUSH;
        } else if (density > 0.45f) {
            type = TERRAIN_OBJECT_FERN;
        } else if (density > 0.35f) {
            type = TERRAIN_OBJECT_GRASS;
        } else if (density > 0.25f) {
            type = TERRAIN_OBJECT_PEBBLE_ROUND;
        } else {
            continue;
        }

        if (type == TERRAIN_OBJECT_TREE && slope > 0.3f) continue;
        if (type == TERRAIN_OBJECT_GRASS && density < 0.4f) continue;

        Terrain_Object_Instance_Data object = {};

        object.type     = type;
        object.position = position;

        object.rotation = random_float(0.0f, 360.0f);

        switch (type) {
            case TERRAIN_OBJECT_TREE: {
                object.scale = random_float(0.8f, 1.5f);
                object.scale *= 5.0f;
            } break;

            case TERRAIN_OBJECT_BUSH: {
                object.scale = random_float(0.6f, 1.2f);
                object.scale *= 0.5f;
            } break;

            case TERRAIN_OBJECT_GRASS: {
                object.scale = random_float(0.5f, 1.0f);
            } break;

            case TERRAIN_OBJECT_FERN: {
                object.scale = 0.5f;
            } break;

            default: {
                object.scale = random_float(0.5f, 1.0f);
            } break;
        }

        object.variation = rand() % num_variations_per_terrain_object_type[type];
        object.mesh      = get_mesh_for_terrain_object_type(type, object.variation);

        objects.push_back(object);
    }

    //
    // Sort objects by type and variation and build batches
    //
    eastl::sort(objects.begin(), objects.end(), [](Terrain_Object_Instance_Data const &a, Terrain_Object_Instance_Data const &b) {
        if (a.type != b.type) return a.type < b.type;
        return a.variation < b.variation;
    });

    batches.clear();
    if (objects.empty()) return false;

    int start_index = 0;
    for (int i = 1; i <= objects.size(); i++) {
        if (i == objects.size() ||
            objects[i].type != objects[start_index].type ||
            objects[i].variation != objects[start_index].variation) {
            Terrain_Object_Instance_Batch batch;

            batch.mesh        = objects[start_index].mesh;
            batch.type        = objects[start_index].type;
            batch.variation   = objects[start_index].variation;
            batch.start_index = start_index;
            batch.count       = i - start_index;

            batches.push_back(batch);

            start_index = i;
        }
    }

    objects_instance_data.resize(objects.size());
    for (int i = 0; i < Render_Backend::MAX_FRAMES_IN_FLIGHT * Scene_Renderer::MAX_RENDER_PASSES; i++) {
        if (!globals.render_backend->create_buffer(&instance_buffers[i], VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, objects.size() * sizeof(Per_Object_Uniforms), NULL)) {
            return false;
        }
    }
    
    for (auto &batch : batches) {
        for (int i = batch.start_index; i < batch.start_index + batch.count; i++) {
            auto &object = objects[i];

            glm::mat4 world_matrix = glm::mat4(1.0f);
            world_matrix = glm::translate(world_matrix, object.position);
            world_matrix = glm::rotate(world_matrix, object.rotation, glm::vec3(0, 1, 0));
            world_matrix = glm::scale(world_matrix, glm::vec3(object.scale));

            objects_instance_data[i].world_matrix         = world_matrix;
            objects_instance_data[i].scale_color          = glm::vec4(1);
        }
    }
    
    return true;
}
