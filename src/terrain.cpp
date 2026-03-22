#include "pch.h"
#include "terrain.h"

float Terrain_Chunk::get_height(float x, float z) {
    float frequency = 0.05f;
    float amplitude = 10.0f;

    return glm::sin(x * frequency) * glm::cos(z * frequency) * amplitude;
}

void Terrain_Chunk::generate(int _num_vertices_per_side, float _scale, glm::vec3 _offset) {
    num_vertices_per_side = _num_vertices_per_side;
    scale                 = _scale;
    offset                = _offset;

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
}
