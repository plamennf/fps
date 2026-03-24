#include "pch.h"
#include "mesh.h"
//#include "texture_catalog.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <stdio.h>

//#define SAVE_GLTF_MESHES_TO_CUSTOM_FORMAT

static char *get_texture_name(cgltf_texture *texture) {
    if (texture->image) {
        char *texture_path_orig = NULL;
        if (texture->image->uri) {
            texture_path_orig = texture->image->uri;
        } else if (texture->image->name) {
            texture_path_orig = texture->image->name;
        }

        if (texture_path_orig) {
            char *texture_name_orig = copy_string(texture_path_orig);
            defer { delete [] texture_name_orig; };
                            
            char *texture_name = strrchr(texture_name_orig, '/');
            if (texture_name) {
                texture_name++;
            } else {
                texture_name = texture_name_orig;
            }

            char *texture_name_end = strrchr(texture_name, '.');
            if (texture_name_end) {
                texture_name[texture_name_end - texture_name] = 0;
            }

            return copy_string(texture_name);
        } else {
            return NULL;
        }
    }

    return NULL;
}

bool load_mesh_gltf(Mesh *mesh, char *filepath) {    
    cgltf_options options = {};
    cgltf_data *data = NULL;
    //if (cgltf_parse_file(&options, file_data, file_data_size, &data) != cgltf_result_success) {
    if (cgltf_parse_file(&options, filepath, &data) != cgltf_result_success) {
        logprintf("Failed to parse gltf file '%s'.\n", filepath);
        return false;
    }
    defer { cgltf_free(data); };
    if (cgltf_load_buffers(&options, data, filepath) != cgltf_result_success) {
        logprintf("Failed to load buffers for: '%s'\n", filepath);
        return false;
    }
    
    mesh->num_submeshes = 0;
    for (int i = 0; i < data->nodes_count; i++) {
        cgltf_node *node = &data->nodes[i];
        if (!node->mesh) continue;
        
        if (node->name) {
            if (strstr(node->name, "Plane") || strstr(node->name, "Proxy") || strstr(node->name, "Shadow")) {
                continue; 
            }
        }
        
        cgltf_mesh *gltf_mesh = data->nodes[i].mesh;
        if (gltf_mesh) {
            mesh->num_submeshes += (int)gltf_mesh->primitives_count;
        }
    }

    mesh->submeshes = new Submesh[mesh->num_submeshes];

    int index = 0;
    for (int n = 0; n < data->nodes_count; n++) {
        cgltf_node *node = &data->nodes[n];
        if (!node->mesh) continue;

        if (node->name) {
            if (strstr(node->name, "Plane") || strstr(node->name, "Proxy") || strstr(node->name, "Shadow")) {
                continue; 
            }
        }

        float fmatrix[16];
        cgltf_node_transform_world(node, fmatrix);

        glm::mat4 matrix;
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                matrix[x][y] = fmatrix[x * 4 + y];
            }
        }
        
        cgltf_mesh *gltf_mesh = node->mesh;
        for (int p = 0; p < gltf_mesh->primitives_count; p++) {
            cgltf_primitive *primitive = &gltf_mesh->primitives[p];
            Submesh *submesh = &mesh->submeshes[index++];

            cgltf_accessor *position = NULL;
            cgltf_accessor *normal   = NULL;
            cgltf_accessor *uv       = NULL;
            cgltf_accessor *tangent  = NULL;
            cgltf_accessor *color    = NULL;
            for (int a = 0; a < primitive->attributes_count; a++) {
                switch (primitive->attributes[a].type) {
                    case cgltf_attribute_type_position: {
                        position = primitive->attributes[a].data;
                    } break;

                    case cgltf_attribute_type_normal: {
                        normal = primitive->attributes[a].data;
                    } break;

                    case cgltf_attribute_type_texcoord: {
                        if (primitive->attributes[a].index == 0) {
                            uv = primitive->attributes[a].data;
                        }
                    } break;

                    case cgltf_attribute_type_tangent: {
                        tangent = primitive->attributes[a].data;
                    } break;

                    case cgltf_attribute_type_color: {
                        color = primitive->attributes[a].data;
                    } break;
                }
            }
            if (!position) continue;

            submesh->num_vertices = (int)position->count;
            submesh->vertices = new Mesh_Vertex[submesh->num_vertices];
            
            for (int i = 0; i < submesh->num_vertices; i++) {
                float p_[3] = {};
                float n_[3] = {};
                float u_[2] = {};
                float t_[3] = {};
                float c_[4] = {};
                cgltf_accessor_read_float(position, i, p_, 3);
                if (normal)  cgltf_accessor_read_float(normal,  i, n_, 3);
                if (uv)      cgltf_accessor_read_float(uv,      i, u_, 2);
                if (tangent) cgltf_accessor_read_float(tangent, i, t_, 3);
                if (color)   cgltf_accessor_read_float(color,   i, c_, 4);

                glm::vec4 position4 = matrix * glm::vec4(p_[0], p_[1], p_[2], 1.0f);
                submesh->vertices[i].position = glm::vec3(position4.x, position4.y, position4.z);
                
                if (color) {
                    submesh->vertices[i].color     = glm::vec4(c_[0], c_[1], c_[2], c_[3]);
                } else {
                    submesh->vertices[i].color     = glm::vec4(1, 1, 1, 1);
                }
                
                if (normal) {
                    glm::vec4 normal4 = matrix * glm::vec4(n_[0], n_[1], n_[2], 0.0f);
                    submesh->vertices[i].normal    = glm::vec3(normal4.x, normal4.y, normal4.z);
                } else {
                    submesh->vertices[i].normal    = glm::vec3(0, 1, 0);
                }

                if (uv) {
                    submesh->vertices[i].uv        = glm::vec2(u_[0], u_[1]);
                } else {
                    submesh->vertices[i].uv        = glm::vec2(0, 0);
                }

                if (tangent) {
                    submesh->vertices[i].tangent   = glm::vec3(t_[0], t_[1], t_[2]);
                    submesh->vertices[i].bitangent = glm::cross(submesh->vertices[i].tangent, submesh->vertices[i].normal);
                } else {
                    submesh->vertices[i].tangent   = glm::vec3(0, 0, 0);
                    submesh->vertices[i].bitangent = glm::vec3(0, 0, 0);
                }
            }

            submesh->num_indices = primitive->indices ? (int)primitive->indices->count : submesh->num_vertices;
            submesh->indices = new u32[submesh->num_indices];
            for (int i = 0; i < submesh->num_indices; i++) {
                if (primitive->indices) {
                    submesh->indices[i] = (u32)cgltf_accessor_read_index(primitive->indices, i);
                } else {
                    submesh->indices[i] = (u32)i;
                }
            }

            submesh->has_gpu_data = false;
            
            /*
              submesh->vertex_buffer = make_gpu_buffer(GPU_BUFFER_VERTEX, submesh->num_vertices * sizeof(Mesh_Vertex), submesh->vertices, false);
              submesh->index_buffer = make_gpu_buffer(GPU_BUFFER_INDEX, submesh->num_indices * sizeof(u32), submesh->indices, false);
            */
            
            if (primitive->material) {
                cgltf_material *material = primitive->material;

                submesh->material.name = copy_string(material->name);
                
                submesh->material.albedo_factor = glm::vec4(1, 1, 1, 1);

                if (material->has_pbr_specular_glossiness) {
                    float *d = material->pbr_specular_glossiness.diffuse_factor;
                    submesh->material.albedo_factor = glm::vec4(d[0], d[1], d[2], d[3]);

                    if (material->pbr_specular_glossiness.diffuse_texture.texture) {
                        cgltf_texture *texture = material->pbr_specular_glossiness.diffuse_texture.texture;
                        submesh->material.albedo_texture_name = get_texture_name(texture);
                    }

                    if (material->pbr_specular_glossiness.specular_glossiness_texture.texture) {
                        cgltf_texture *texture = material->pbr_specular_glossiness.specular_glossiness_texture.texture;
                        submesh->material.metallic_roughness_texture_name = get_texture_name(texture);
                    }

                    submesh->material.uses_specular_glossiness = true;
                } else if (material->has_pbr_metallic_roughness) {
                    float *b = material->pbr_metallic_roughness.base_color_factor;
                    submesh->material.albedo_factor = glm::vec4(b[0], b[1], b[2], b[3]);
                    
                    if (material->pbr_metallic_roughness.base_color_texture.texture) {
                        cgltf_texture *texture = material->pbr_metallic_roughness.base_color_texture.texture;
                        submesh->material.albedo_texture_name = get_texture_name(texture);
                    }

                    if (material->pbr_metallic_roughness.metallic_roughness_texture.texture) {
                        cgltf_texture *texture = material->pbr_metallic_roughness.metallic_roughness_texture.texture;
                        submesh->material.metallic_roughness_texture_name = get_texture_name(texture);
                    }

                    submesh->material.uses_specular_glossiness = false;
                }
                
                if (material->normal_texture.texture) {
                    cgltf_texture *texture = material->normal_texture.texture;
                    submesh->material.normal_texture_name = get_texture_name(texture);
                }

                if (material->occlusion_texture.texture) {
                    cgltf_texture *texture = material->occlusion_texture.texture;
                    submesh->material.ao_texture_name = get_texture_name(texture);
                }

                if (material->emissive_texture.texture) {
                    cgltf_texture *texture = material->emissive_texture.texture;
                    submesh->material.emissive_texture_name = get_texture_name(texture);

                    float *e = material->emissive_factor;
                    submesh->material.emissive_factor = glm::vec3(e[0], e[1], e[2]);
                }
            }
        }
    }

    char *base_filepath = copy_string(filepath);
    defer { delete [] base_filepath; };
    char *extension = strrchr(base_filepath, '.');
    if (extension) {
        base_filepath[extension - base_filepath] = 0;
    }
    
#ifdef SAVE_GLTF_MESHES_TO_CUSTOM_FORMAT
    char new_filepath[4096];
    snprintf(new_filepath, sizeof(new_filepath), "%s.mesh", base_filepath);
    save_mesh(mesh, new_filepath);
#endif
    
    return true;
}

static void save_binary_string(FILE *file, char *s) {
    int len = (int)string_length(s);
    fwrite(&len, sizeof(int), 1, file);
    fwrite(s, sizeof(char), len, file);
}

bool save_mesh(Mesh *mesh, char *filepath) {
    FILE *file = fopen(filepath, "wb");
    if (!file) {
        logprintf("Failed to open file '%s' for writing!\n", filepath);
        return false;
    }
    defer { fclose(file); };

    int version = MESH_FILE_VERSION;
    fwrite(&version, sizeof(int), 1, file);

    fwrite(&mesh->num_submeshes, sizeof(int), 1, file);
    for (int i = 0; i < mesh->num_submeshes; i++) {
        Submesh *submesh = &mesh->submeshes[i];
        
        fwrite(&submesh->num_vertices, sizeof(int), 1, file);
        fwrite(submesh->vertices, sizeof(Mesh_Vertex), submesh->num_vertices, file);

        fwrite(&submesh->num_indices, sizeof(int), 1, file);
        fwrite(submesh->indices, sizeof(u32), submesh->num_indices, file);

#if 0
        if (submesh->material.diffuse_texture_name) {
            save_binary_string(file, submesh->material.diffuse_texture_name);
        } else {
            save_binary_string(file, "-");
        }

        if (submesh->material.specular_texture_name) {
            save_binary_string(file, submesh->material.specular_texture_name);
        } else {
            save_binary_string(file, "-");
        }

        if (submesh->material.normal_texture_name) {
            save_binary_string(file, submesh->material.normal_texture_name);
        } else {
            save_binary_string(file, "-");
        }
#endif
        
        fwrite(&submesh->material.albedo_factor.x, sizeof(float), 4, file);
    }

    fclose(file);

    return true;
}

bool load_mesh_custom(Mesh *mesh, char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        logprintf("Failed to open file '%s' for reading!\n", filepath);
        return false;
    }
    defer { fclose(file); };

    int version = 0;
    fread(&version, sizeof(int), 1, file);

    if (version <= 0 || version > MESH_FILE_VERSION) {
        logprintf("Invalid version number for mesh '%s'\n", filepath);
        return false;
    }

    fread(&mesh->num_submeshes, sizeof(int), 1, file);
    assert(mesh->num_submeshes > 0);
    mesh->submeshes = new Submesh[mesh->num_submeshes];
    for (int i = 0; i < mesh->num_submeshes; i++) {
        Submesh *submesh = &mesh->submeshes[i];

        fread(&submesh->num_vertices, sizeof(int), 1, file);
        assert(submesh->num_vertices > 0);
        submesh->vertices = new Mesh_Vertex[submesh->num_vertices];
        fread(submesh->vertices, sizeof(Mesh_Vertex), submesh->num_vertices, file);

        fread(&submesh->num_indices, sizeof(int), 1, file);
        assert(submesh->num_indices > 0);
        submesh->indices = new u32[submesh->num_indices];
        fread(submesh->indices, sizeof(u32), submesh->num_indices, file);

        /*
        submesh->vertex_buffer = make_gpu_buffer(GPU_BUFFER_VERTEX, submesh->num_vertices * sizeof(Mesh_Vertex), submesh->vertices, false);
        submesh->index_buffer = make_gpu_buffer(GPU_BUFFER_INDEX, submesh->num_indices * sizeof(u32), submesh->indices, false);
        */

#if 0
        int diffuse_texture_filepath_len = 0;
        fread(&diffuse_texture_filepath_len, sizeof(int), 1, file);
        char *diffuse_texture_filepath = new char[diffuse_texture_filepath_len + 1];
        defer { delete [] diffuse_texture_filepath; };
        fread(diffuse_texture_filepath, sizeof(char), diffuse_texture_filepath_len, file);
        diffuse_texture_filepath[diffuse_texture_filepath_len] = 0;

        if (diffuse_texture_filepath[0] == '-') {
            submesh->material.diffuse_texture_name = NULL;
        } else {
            submesh->material.diffuse_texture_name = copy_string(diffuse_texture_filepath);
        }

        int specular_texture_filepath_len = 0;
        fread(&specular_texture_filepath_len, sizeof(int), 1, file);
        char *specular_texture_filepath = new char[specular_texture_filepath_len + 1];
        defer { delete [] specular_texture_filepath; };
        fread(specular_texture_filepath, sizeof(char), specular_texture_filepath_len, file);
        specular_texture_filepath[specular_texture_filepath_len] = 0;

        if (specular_texture_filepath[0] == '-') {
            submesh->material.specular_texture_name = NULL;
        } else {
            submesh->material.specular_texture_name = copy_string(specular_texture_filepath);
        }

        int normal_texture_filepath_len = 0;
        fread(&normal_texture_filepath_len, sizeof(int), 1, file);
        char *normal_texture_filepath = new char[normal_texture_filepath_len + 1];
        defer { delete [] normal_texture_filepath; };
        fread(normal_texture_filepath, sizeof(char), normal_texture_filepath_len, file);
        normal_texture_filepath[normal_texture_filepath_len] = 0;

        if (normal_texture_filepath[0] == '-') {
            submesh->material.normal_texture_name = NULL;
        } else {
            submesh->material.normal_texture_name = copy_string(normal_texture_filepath);
        }
#endif

        fread(&submesh->material.albedo_factor.x, sizeof(float), 4, file);
    }
    
    fclose(file);
    
    return true;
}

bool load_mesh(Mesh *mesh, char *filepath) {
    const char *extension = strrchr(filepath, '.');
    if (!extension) return false;
    extension++;

    if (strings_match(extension, "gltf") || strings_match(extension, "glb")) {
        return load_mesh_gltf(mesh, filepath);
    } else if (strings_match(extension, "mesh")) {
        return load_mesh_custom(mesh, filepath);
    }

    float max_distance_squared = 0.0f;
    for (int i = 0; i < mesh->num_submeshes; i++) {
        Submesh *submesh = &mesh->submeshes[i];
        for (int j = 0; j < submesh->num_vertices; j++) {
            Mesh_Vertex const &vertex = submesh->vertices[j];
            float distance_squared = glm::length2(vertex.position);
            if (distance_squared > max_distance_squared) {
                max_distance_squared = distance_squared;
            }
        }
    }

    mesh->bounds_radius = sqrtf(max_distance_squared);
    
    return false;
}
