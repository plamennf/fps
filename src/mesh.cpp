#include "main.h"
#include "mesh.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <stdio.h>

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
    for (int i = 0; i < data->meshes_count; i++) {
        mesh->num_submeshes += (int)data->meshes[i].primitives_count;
    }

    mesh->submeshes = new Submesh[mesh->num_submeshes];

    int index = 0;
    for (int m = 0; m < data->meshes_count; m++) {
        cgltf_mesh *gltf_mesh = &data->meshes[m];
        for (int p = 0; p < gltf_mesh->primitives_count; p++) {
            cgltf_primitive *primitive = &gltf_mesh->primitives[p];
            Submesh *submesh = &mesh->submeshes[index++];

            cgltf_accessor *position = NULL;
            cgltf_accessor *normal   = NULL;
            cgltf_accessor *uv       = NULL;
            cgltf_accessor *tangent  = NULL;
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
                cgltf_accessor_read_float(position, i, p_, 3);
                if (normal)  cgltf_accessor_read_float(normal,  i, n_, 3);
                if (uv)      cgltf_accessor_read_float(uv,      i, u_, 2);
                if (tangent) cgltf_accessor_read_float(tangent, i, t_, 3);

                submesh->vertices[i].position = v3(p_[0], p_[1], p_[2]);

                if (normal) {
                    submesh->vertices[i].normal    = v3(n_[0], n_[1], n_[2]);
                } else {
                    submesh->vertices[i].normal    = v3(0, 1, 0);
                }

                if (uv) {
                    submesh->vertices[i].uv        = v2(u_[0], u_[1]);
                } else {
                    submesh->vertices[i].uv        = v2(0, 0);
                }

                if (tangent) {
                    submesh->vertices[i].tangent   = v3(t_[0], t_[1], t_[2]);
                    submesh->vertices[i].bitangent = cross_product(submesh->vertices[i].tangent, submesh->vertices[i].normal);
                } else {
                    submesh->vertices[i].tangent   = v3(0, 0, 0);
                    submesh->vertices[i].bitangent = v3(0, 0, 0);
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

            submesh->vertex_buffer = make_gpu_buffer(GPU_BUFFER_VERTEX, submesh->num_vertices * sizeof(Mesh_Vertex), submesh->vertices, false);
            submesh->index_buffer = make_gpu_buffer(GPU_BUFFER_INDEX, submesh->num_indices * sizeof(u32), submesh->indices, false);
            
            if (primitive->material) {
                cgltf_material *material = primitive->material;

                if (material->pbr_metallic_roughness.base_color_texture.texture) {
                    cgltf_texture *texture = material->pbr_metallic_roughness.base_color_texture.texture;
                    if (texture->image) {
                        char *texture_path_orig = NULL;
                        if (texture->image->uri) {
                            texture_path_orig = copy_string(texture->image->uri);
                        } else if (texture->image->name) {
                            bool has_extension = true;
                            s64 new_string_len = string_length(texture->image->name) + 1;
                            if (!strstr(texture->image->name, ".")) {
                                new_string_len += 4;
                                has_extension = false;
                            }
                            
                            texture_path_orig = new char[new_string_len];
                            if (has_extension) {
                                memcpy(texture_path_orig, texture->image->name, new_string_len);
                            } else {
                                memcpy(texture_path_orig, texture->image->name, string_length(texture->image->name));
                                memcpy(texture_path_orig + string_length(texture->image->name), ".png", 5);
                            }
                        }

                        if (texture_path_orig) {
                            defer { delete [] texture_path_orig; };
                            
                            // TODO: Give these variables better names.
                            char *texture_path = strrchr(texture_path_orig, '/');
                            if (texture_path) {
                                texture_path++;
                            } else {
                                texture_path = texture_path_orig;
                            }

                            char full_path[1024];
                            snprintf(full_path, sizeof(full_path), "data/textures/%s", texture_path);
                        
                            submesh->material.diffuse_texture = load_texture(full_path);
                        } else {
                            submesh->material.diffuse_texture = globals.white_texture;
                        }
                    }
                } else {
                    submesh->material.diffuse_texture = globals.white_texture;
                }

                if (material->pbr_specular_glossiness.diffuse_texture.texture && submesh->material.diffuse_texture == globals.white_texture) {
                    cgltf_texture *texture = material->pbr_specular_glossiness.diffuse_texture.texture;
                    if (texture->image) {
                        char *texture_path_orig = NULL;
                        if (texture->image->uri) {
                            texture_path_orig = copy_string(texture->image->uri);
                        } else if (texture->image->name) {
                            bool has_extension = true;
                            s64 new_string_len = string_length(texture->image->name) + 1;
                            if (!strstr(texture->image->name, ".")) {
                                new_string_len += 4;
                                has_extension = false;
                            }
                            
                            texture_path_orig = new char[new_string_len];
                            if (has_extension) {
                                memcpy(texture_path_orig, texture->image->name, new_string_len);
                            } else {
                                memcpy(texture_path_orig, texture->image->name, string_length(texture->image->name));
                                memcpy(texture_path_orig + string_length(texture->image->name), ".png", 5);
                            }
                        }

                        if (texture_path_orig) {
                            defer { delete [] texture_path_orig; };
                            
                            // TODO: Give these variables better names.
                            char *texture_path = strrchr(texture_path_orig, '/');
                            if (texture_path) {
                                texture_path++;
                            } else {
                                texture_path = texture_path_orig;
                            }

                            char full_path[1024];
                            snprintf(full_path, sizeof(full_path), "data/textures/%s", texture_path);
                        
                            submesh->material.diffuse_texture = load_texture(full_path);
                        } else {
                            submesh->material.diffuse_texture = globals.white_texture;
                        }
                    }
                } else {
                    //submesh->material.diffuse_texture = globals.white_texture;
                }

                if (material->specular.specular_texture.texture) {
                    cgltf_texture *texture = material->specular.specular_texture.texture;
                    if (texture->image && texture->image->uri) {
                        char *texture_path_orig = copy_string(texture->image->uri);
                        defer { delete [] texture_path_orig; };

                        // TODO: Give these variables better names.
                        char *texture_path = strrchr(texture_path_orig, '/');
                        if (texture_path) {
                            texture_path++;
                        } else {
                            texture_path = texture_path_orig;
                        }

                        char full_path[1024];
                        snprintf(full_path, sizeof(full_path), "data/textures/%s", texture_path);
                        
                        submesh->material.specular_texture = load_texture(full_path);
                    } else {
                        submesh->material.specular_texture = globals.white_texture;
                    }
                } else {
                    submesh->material.specular_texture = globals.white_texture;
                }

                if (material->normal_texture.texture) {
                    cgltf_texture *texture = material->normal_texture.texture;
                    if (texture->image && texture->image->uri) {
                        char *texture_path_orig = copy_string(texture->image->uri);
                        defer { delete [] texture_path_orig; };

                        // TODO: Give these variables better names.
                        char *texture_path = strrchr(texture_path_orig, '/');
                        if (texture_path) {
                            texture_path++;
                        } else {
                            texture_path = texture_path_orig;
                        }

                        char full_path[1024];
                        snprintf(full_path, sizeof(full_path), "data/textures/%s", texture_path);
                        
                        submesh->material.normal_texture = load_texture(full_path);
                    } else {
                        submesh->material.normal_texture = NULL;
                    }
                } else {
                    submesh->material.normal_texture = NULL;
                }

                Vector4 gltf_base_color_factor;
                memcpy(&gltf_base_color_factor.x, material->pbr_metallic_roughness.base_color_factor, 4 * sizeof(float));
                if (length(gltf_base_color_factor) > 1e-6) {
                    submesh->material.diffuse_color = gltf_base_color_factor;
                } else {
                    submesh->material.diffuse_color = v4(1, 1, 1, 1);
                }
                
                float r = material->pbr_metallic_roughness.roughness_factor;
                submesh->material.shininess = 2.0f/(r*r) - 2.0f;
                clamp(&submesh->material.shininess, 1.0f, 256.0f);
            }
        }
    }

    char *base_filepath = copy_string(filepath);
    defer { delete [] base_filepath; };
    char *extension = strrchr(base_filepath, '.');
    if (extension) {
        base_filepath[extension - base_filepath] = 0;
    }

    char new_filepath[4096];
    snprintf(new_filepath, sizeof(new_filepath), "%s.mesh", base_filepath);
    save_mesh(mesh, new_filepath);

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

        if (submesh->material.diffuse_texture && submesh->material.diffuse_texture != globals.white_texture) {
            save_binary_string(file, submesh->material.diffuse_texture->filepath);
        } else {
            save_binary_string(file, "-");
        }

        if (submesh->material.specular_texture && submesh->material.specular_texture != globals.white_texture) {
            save_binary_string(file, submesh->material.specular_texture->filepath);
        } else {
            save_binary_string(file, "-");
        }

        if (submesh->material.normal_texture) {
            save_binary_string(file, submesh->material.normal_texture->filepath);
        } else {
            save_binary_string(file, "-");
        }

        fwrite(&submesh->material.diffuse_color.x, sizeof(float), 4, file);
        fwrite(&submesh->material.shininess, sizeof(float), 1, file);
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

        submesh->vertex_buffer = make_gpu_buffer(GPU_BUFFER_VERTEX, submesh->num_vertices * sizeof(Mesh_Vertex), submesh->vertices, false);
        submesh->index_buffer = make_gpu_buffer(GPU_BUFFER_INDEX, submesh->num_indices * sizeof(u32), submesh->indices, false);
        
        int diffuse_texture_filepath_len = 0;
        fread(&diffuse_texture_filepath_len, sizeof(int), 1, file);
        char *diffuse_texture_filepath = new char[diffuse_texture_filepath_len + 1];
        defer { delete [] diffuse_texture_filepath; };
        fread(diffuse_texture_filepath, sizeof(char), diffuse_texture_filepath_len, file);
        diffuse_texture_filepath[diffuse_texture_filepath_len] = 0;

        if (diffuse_texture_filepath[0] == '-') {
            submesh->material.diffuse_texture = globals.white_texture;
        } else {
            submesh->material.diffuse_texture = load_texture(diffuse_texture_filepath);
        }

        int specular_texture_filepath_len = 0;
        fread(&specular_texture_filepath_len, sizeof(int), 1, file);
        char *specular_texture_filepath = new char[specular_texture_filepath_len + 1];
        defer { delete [] specular_texture_filepath; };
        fread(specular_texture_filepath, sizeof(char), specular_texture_filepath_len, file);
        specular_texture_filepath[specular_texture_filepath_len] = 0;

        if (specular_texture_filepath[0] == '-') {
            submesh->material.specular_texture = globals.white_texture;
        } else {
            submesh->material.specular_texture = load_texture(specular_texture_filepath);
        }

        int normal_texture_filepath_len = 0;
        fread(&normal_texture_filepath_len, sizeof(int), 1, file);
        char *normal_texture_filepath = new char[normal_texture_filepath_len + 1];
        defer { delete [] normal_texture_filepath; };
        fread(normal_texture_filepath, sizeof(char), normal_texture_filepath_len, file);
        normal_texture_filepath[normal_texture_filepath_len] = 0;

        if (normal_texture_filepath[0] == '-') {
            submesh->material.normal_texture = NULL;
        } else {
            submesh->material.normal_texture = load_texture(normal_texture_filepath);
        }

        fread(&submesh->material.diffuse_color.x, sizeof(float), 4, file);
        fread(&submesh->material.shininess, sizeof(float), 1, file);
    }

    fclose(file);
    
    return true;
}

bool load_mesh(Mesh *mesh, char *filepath) {
    char *extension = strrchr(filepath, '.');
    if (!extension) return false;
    extension++;

    if (strings_match(extension, "gltf") || strings_match(extension, "glb")) {
        return load_mesh_gltf(mesh, filepath);
    } else if (strings_match(extension, "mesh")) {
        return load_mesh_custom(mesh, filepath);
    }

    return false;
}
