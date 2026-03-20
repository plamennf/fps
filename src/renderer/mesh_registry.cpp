#include "pch.h"
#include "mesh_registry.h"
#include "mesh.h"
#include "render_backend.h"

#include <stdio.h>

#include <filesystem>

// TODO: Copy-paste: Move this into corelib
static bool file_exists(char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

Mesh *Mesh_Registry::find_or_load(char *name) {
    auto it = mesh_lookup.find(name);
    if (it != mesh_lookup.end()) {
        return it->second;
    }

#if 0
    char gltf_full_path[4096];
    snprintf(gltf_full_path, sizeof(gltf_full_path), "%s/%s.glb", MESH_DIRECTORY, name);
    u64 gltf_full_path_modtime = 0;
    if (!os_file_exists(gltf_full_path)) {
        snprintf(gltf_full_path, sizeof(gltf_full_path), "%s/%s.gltf", MESH_DIRECTORY, name);
        if (!file_exists(gltf_full_path)) {
            logprintf("No gltf file named '%s' found in '%s'\n", MESH_DIRECTORY);
            return NULL;
        }
    }
    os_get_file_last_write_time(gltf_full_path, &gltf_full_path_modtime);
    
    char mesh_full_path[4096];
    snprintf(mesh_full_path, sizeof(mesh_full_path), "%s/%s.mesh", MESH_DIRECTORY, name);
    u64 mesh_full_path_modtime = 0;
    if (file_exists(mesh_full_path)) {
        os_get_file_last_write_time(mesh_full_path, &mesh_full_path_modtime);
    }

    char *full_path = NULL;
    if (gltf_full_path_modtime > mesh_full_path_modtime) {
        full_path = gltf_full_path;
    } else {
        full_path = mesh_full_path;
    }
#else
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "data/meshes/%s.gltf", name);

    if (!file_exists(full_path)) {
        snprintf(full_path, sizeof(full_path), "data/meshes/%s.glb", name);
        if (!file_exists(full_path)) {
            logprintf("No mesh '%s' found in 'data/meshes'\n", name);
            return NULL;
        }
    }

#endif

    Mesh *mesh = new Mesh();
    if (!load_mesh(mesh, full_path)) {
        delete mesh;
        return NULL;
    }

    /*
    if (!generate_gpu_data_for_mesh(mesh)) {
        delete mesh;
        return NULL;
    }
    */
    
    mesh_lookup.insert({name, mesh});
    all_names_in_order_of_loading.push_back(copy_string(name));
    
    return mesh;
}

void Mesh_Registry::recursive_init_all() {
    for (const auto &entry : std::filesystem::recursive_directory_iterator("data/meshes")) {
        if (entry.is_regular_file()) {
            if (entry.path().extension() != ".gltf") continue;
            
            auto cpp_filename = entry.path().relative_path().string();
            char *filename = (char *)cpp_filename.c_str();
            
            filename = copy_string(filename);
            defer { delete [] filename; };

            char *start = find_character_from_left(filename, '\\');
            if (start) {
                start++;
            }

            char *slash = strrchr(filename, '.');
            if (slash) {
                filename[slash - filename] = 0;
            }

            for (char *at = (char *)start; *at; at++) {
                if (*at == '\\') {
                    *at = '/';
                }
            }
            
            if (!find_or_load(start)) {
                logprintf("Failed to load '%s'\n", start);
            }
        }
    }
}
