#include "general.h"
#include "mesh_catalog.h"
#include "os_specific.h"
#include "mesh.h"

#include <stdio.h>

#define MESH_DIRECTORY "data/meshes"

Mesh *Mesh_Catalog::find_or_load(char *name) {
    Mesh **_mesh = mesh_lookup.find(name);
    if (_mesh) return *_mesh;
    
    char gltf_full_path[4096];
    snprintf(gltf_full_path, sizeof(gltf_full_path), "%s/%s.glb", MESH_DIRECTORY, name);
    u64 gltf_full_path_modtime = 0;
    if (!os_file_exists(gltf_full_path)) {
        snprintf(gltf_full_path, sizeof(gltf_full_path), "%s/%s.gltf", MESH_DIRECTORY, name);
        if (!os_file_exists(gltf_full_path)) {
            logprintf("No gltf file named '%s' found in '%s'\n", MESH_DIRECTORY);
            return NULL;
        }
        os_get_file_last_write_time(gltf_full_path, &gltf_full_path_modtime);
    }
    
    char mesh_full_path[4096];
    snprintf(mesh_full_path, sizeof(mesh_full_path), "%s/%s.mesh", MESH_DIRECTORY, name);
    u64 mesh_full_path_modtime = 0;
    if (os_file_exists(mesh_full_path)) {
        os_get_file_last_write_time(mesh_full_path, &mesh_full_path_modtime);
    }

    char *full_path = NULL;
    if (gltf_full_path_modtime > mesh_full_path_modtime) {
        full_path = gltf_full_path;
    } else {
        full_path = mesh_full_path;
    }

    Mesh *mesh = new Mesh();
    if (!load_mesh(mesh, full_path)) {
        delete mesh;
        return NULL;
    }
    mesh_lookup.add(name, mesh);
    return mesh;
}
