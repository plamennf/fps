#include "general.h"
#include "mesh_catalog.h"
#include "os_specific.h"
#include "mesh.h"
#include "renderer.h"
#include "job_system.h"

#include <stdio.h>

#define MESH_DIRECTORY "data/meshes"

struct Mesh_Load_Job {
    Mesh_Entry *entry;
    char filepath[1024];
};

static void mesh_load_job(void *ptr) {
    Mesh_Load_Job *job = (Mesh_Load_Job *)ptr;

    logprintf("Mesh load job!!!\n");
    
    if (!load_mesh(job->entry->mesh, job->filepath)) {
        logprintf("Filepath: %s\n", job->filepath);
        job->entry->state = MESH_STATE_FAILED;
    } else {
        job->entry->state = MESH_STATE_CPU_READY;
    }
}

Mesh *Mesh_Catalog::find_or_load(char *name) {
    Mesh_Entry *_entry = mesh_lookup.find(name);
    if (_entry) {
        if (_entry->state == MESH_STATE_READY) {
            return _entry->mesh;
        }
        return NULL;
    }
    
    char gltf_full_path[4096];
    snprintf(gltf_full_path, sizeof(gltf_full_path), "%s/%s.glb", MESH_DIRECTORY, name);
    u64 gltf_full_path_modtime = 0;
    if (!os_file_exists(gltf_full_path)) {
        snprintf(gltf_full_path, sizeof(gltf_full_path), "%s/%s.gltf", MESH_DIRECTORY, name);
        if (!os_file_exists(gltf_full_path)) {
            logprintf("No gltf file named '%s' found in '%s'\n", MESH_DIRECTORY);
            return NULL;
        }
    }
    os_get_file_last_write_time(gltf_full_path, &gltf_full_path_modtime);
    
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

    Mesh_Entry entry = {};
    entry.mesh  = new Mesh();
    entry.state = MESH_STATE_LOADING;

    s64 name_length = string_length(name);
    for (int i = 0; i < name_length; i++) {
        entry.filepath[i] = name[i];
    }
    entry.filepath[name_length] = 0;

    mesh_lookup.add(name, entry);

    Mesh_Load_Job *job = new Mesh_Load_Job();
    job->entry = mesh_lookup.find(name);
    memcpy(job->filepath, full_path, string_length(full_path) + 1);

    enqueue_job(mesh_load_job, job);
    
    return NULL;
}

static void upload_mesh_buffers_to_gpu(Mesh *mesh) {
    logprintf("Uploading mesh buffers to gpu!!!\n");
    
    for (int i = 0; i < mesh->num_submeshes; i++) {
        Submesh *submesh = &mesh->submeshes[i];

        submesh->vertex_buffer = make_gpu_buffer(GPU_BUFFER_VERTEX, submesh->num_vertices * sizeof(Mesh_Vertex), submesh->vertices, false);
        submesh->index_buffer = make_gpu_buffer(GPU_BUFFER_INDEX, submesh->num_indices * sizeof(u32), submesh->indices, false);
    }
}

void Mesh_Catalog::update() {
    for (int i = 0; i < mesh_lookup.allocated; i++) {
        if (!mesh_lookup.occupancy_mask[i]) continue;

        Mesh_Entry *entry = &mesh_lookup.buckets[i].value;

        if (entry->state == MESH_STATE_CPU_READY) {
            upload_mesh_buffers_to_gpu(entry->mesh);
            entry->state = MESH_STATE_READY;
        }
    }
}
