#include "pch.h"
#include "terrain.h"
#include "renderer/mesh_registry.h"
#include "main.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

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

static float sample_height_bilinear(eastl::vector <float> &heightmap_data, int num_vertices_per_side, float fx, float fz) {
    int x0 = (int)floorf(fx);
    int z0 = (int)floorf(fz);
    int x1 = x0 + 1;
    int z1 = z0 + 1;

    x0 = glm::clamp(x0, 0, num_vertices_per_side - 1);
    z0 = glm::clamp(z0, 0, num_vertices_per_side - 1);
    x1 = glm::clamp(x1, 0, num_vertices_per_side - 1);
    z1 = glm::clamp(z1, 0, num_vertices_per_side - 1);

    float tx = fx - x0;
    float tz = fz - z0;

    float h00 = heightmap_data[z0 * num_vertices_per_side + x0];
    float h10 = heightmap_data[z0 * num_vertices_per_side + x1];
    float h01 = heightmap_data[z1 * num_vertices_per_side + x0];
    float h11 = heightmap_data[z1 * num_vertices_per_side + x1];

    float hx0 = glm::mix(h00, h10, tx);
    float hx1 = glm::mix(h01, h11, tx);

    return glm::mix(hx0, hx1, tz);
}

bool Terrain_Chunk::generate(u32 _seed, int _num_vertices_per_side, float _scale, glm::vec3 _offset, int num_objects_to_place) {
    seed                  = _seed;
    num_vertices_per_side = _num_vertices_per_side;
    scale                 = _scale;
    offset                = _offset;

    srand(seed);
    
    eastl::vector <Mesh_Vertex> vertices;
    eastl::vector <u32> indices;
    eastl::vector <float> heightmap_data;
    
    //
    // Generate vertices
    //
    {
        vertices.resize(num_vertices_per_side * num_vertices_per_side);
        heightmap_data.resize(num_vertices_per_side * num_vertices_per_side);
        for (int z = 0; z < num_vertices_per_side; z++) {
            for (int x = 0; x < num_vertices_per_side; x++) {
                int index = z * num_vertices_per_side + x;

                float world_x = x * scale + offset.x;
                float world_z = z * scale + offset.z;

                float height          = get_height(world_x, world_z);
                heightmap_data[index] = height;

                Mesh_Vertex v = {};

                v.position = glm::vec3(world_x, height, world_z);
                v.uv       = glm::vec2((float)x / (num_vertices_per_side - 1), (float)z / (num_vertices_per_side - 1));
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

    if (!globals.render_backend->create_texture(&heightmap, num_vertices_per_side, num_vertices_per_side, VK_FORMAT_R16_SFLOAT, heightmap_data.data(), "heightmap")) {
        return false;
    }

    eastl::vector <float> ao_map_data;
    int ao_map_width = 1024, ao_map_height = ao_map_width;
    ao_map_data.resize(ao_map_width * ao_map_height);
    float scale_factor = (float)(num_vertices_per_side - 1) / (float)(ao_map_width - 1);
    
    int radius      = 8;
    int num_bounces = 64;
    for (int z = 0; z < ao_map_height; z++) {
        for (int x = 0; x < ao_map_width; x++) {
            float hx = x * scale_factor;
            float hz = z * scale_factor;
            
            float current_height = sample_height_bilinear(heightmap_data, num_vertices_per_side, hx, hz);

            float occlusion = 0.0f;
            for (int i = 0; i < num_bounces; i++) {
                float angle = (float)i / num_bounces * glm::two_pi<float>();
                float dx    = glm::cos(angle);
                float dz    = glm::sin(angle);

                float max_slope = -FLT_MAX;
                float max_dist  = -FLT_MAX;
                for (int step = 1; step <= radius; step++) {
                    float t = (float)step / radius;
                    float dist = t * t * radius;
                    
                    float sx = x + dx * dist;
                    float sz = z + dz * dist;

                    float h    = sample_height_bilinear(heightmap_data, num_vertices_per_side, sx, sz);
                    float world_dist = dist * scale;

                    float slope = (h - current_height) / world_dist;
                    slope = glm::clamp(slope, -1.0f, 1.0f);

                    if (slope > max_slope) {
                        max_slope = slope;
                        max_dist  = dist;
                    }
                }

                float weight = 1.0f - (max_dist / radius);
                occlusion += Max(max_slope, 0.0f) * weight;
            }

            occlusion /= num_bounces;

            //float ao = 1.0f - occlusion;//glm::clamp(occlusion + 2.0f, 0.0f, 1.0f);
            float ao = glm::exp(-occlusion * 1.5f);
            ao = glm::clamp(ao, 0.0f, 1.0f);
            ao_map_data[z * ao_map_height + x] = ao;
        }
    }

    float min_ao = 1.0f;
    float max_ao = 0.0f;

    for (float v : ao_map_data) {
        min_ao = glm::min(min_ao, v);
        max_ao = glm::max(max_ao, v);
    }

    logprintf("AO range: %f -> %f\n", min_ao, max_ao);
    
    eastl::vector <float> ao_map_data_blurred;
    ao_map_data_blurred.resize(ao_map_width * ao_map_height);

    eastl::vector <float> temp;
    temp.resize(ao_map_width * ao_map_height);

    float kernel[5] = {0.0625f, 0.25f, 0.375f, 0.25f, 0.0625f};
    
    for (int z = 1; z < ao_map_height - 1; z++) {
        for (int x = 1; x < ao_map_width - 1; x++) {
            float sum = 0.0f;

            for (int k = -2; k <= 2; k++) {
                int sx = glm::clamp(x + k, 0, ao_map_height - 1);
                sum += ao_map_data[z * ao_map_height + sx] * kernel[k + 2];
            }

            temp[z * ao_map_height + x] = sum;
        }
    }

    for (int z = 1; z < ao_map_height - 1; z++) {
        for (int x = 1; x < ao_map_width - 1; x++) {
            float sum = 0.0f;

            for (int k = -2; k <= 2; k++) {
                int sz = glm::clamp(z + k, 0, ao_map_height - 1);
                sum += temp[sz * ao_map_height + x] * kernel[k + 2];
            }

            ao_map_data_blurred[z * ao_map_height + x] = sum;
        }
    }

    eastl::vector <u8> ao_map_data_int;
    ao_map_data_int.resize(ao_map_width * ao_map_height);
    for (int i = 0; i < ao_map_data_blurred.size(); i++) {
        float ao = glm::clamp(ao_map_data_blurred[i], 0.0f, 1.0f);
        ao = glm::pow(ao, 1.5f);
        ao_map_data_int[i] = (u8)(ao * 255.0f);
    }

    stbi_write_bmp("terrain_ao.bmp", ao_map_width, ao_map_height, 1, ao_map_data_int.data());

    if (!globals.render_backend->create_texture(&ao_map, ao_map_width, ao_map_height, VK_FORMAT_R8_UNORM, ao_map_data.data(), "terrain_ao")) {
        return false;
    }
    
    return true;
}
