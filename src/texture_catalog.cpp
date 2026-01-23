#include "general.h"
#include "texture_catalog.h"
#include "os_specific.h"
#include "draw.h"
#include "job_system.h"

#include <stdio.h>
#include <stb_image.h>

#define TEXTURE_DIRECTORY "data/textures"

struct Texture_Load_Job {
    Texture_Entry *entry;
    char filepath[1024];
};

bool load_texture_cpu(Texture_Cpu *cpu, char *filepath) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(1);
    stbi_uc *data = stbi_load(filepath, &width, &height, &channels, 0);
    if (!data) {
        logprintf("Failed to read file '%s'\n", filepath);
        return false;
    }
    //defer { stbi_image_free(data); };

    Texture_Format format = TEXTURE_FORMAT_UNKNOWN;
    switch (channels) {
        case 4: {
            format = TEXTURE_FORMAT_RGBA8;
        } break;

        case 3: {
            format = TEXTURE_FORMAT_RGB8;
        } break;

        case 2: {
            format = TEXTURE_FORMAT_RG8;
        } break;

        case 1: {
            format = TEXTURE_FORMAT_R8;
        } break;
    }

    cpu->width  = width;
    cpu->height = height;
    cpu->format = format;
    cpu->bpp    = channels;
    cpu->data   = data;

    return true;
}

static void texture_load_job(void *ptr) {
    Texture_Load_Job *job = (Texture_Load_Job *)ptr;

    if (!load_texture_cpu(&job->entry->cpu, job->filepath)) {
        job->entry->state = TEXTURE_STATE_FAILED;
    } else {
        job->entry->state = TEXTURE_STATE_CPU_READY;
    }
}

Texture *Texture_Catalog::find_or_load(char *name) {
    Texture_Entry *_entry = texture_lookup.find(name);
    if (_entry) {
        if (_entry->state == TEXTURE_STATE_READY) {
            return _entry->texture;
        }
        return NULL;
    }

    Texture_Entry entry = {};
    entry.texture = NULL;
    entry.cpu = {};
    entry.state = TEXTURE_STATE_LOADING;

    s64 name_length = string_length(name);
    for (int i = 0; i < name_length; i++) {
        entry.filepath[i] = name[i];
    }
    entry.filepath[name_length] = 0;
    
    texture_lookup.add(name, entry);

    char *extensions[] = {
        "png",
        "jpg",
        "jpeg",
        "bmp",
    };

    char full_path[1024];
    bool full_path_exists = false;
    for (int i = 0; i < ArrayCount(extensions); i++) {
        snprintf(full_path, sizeof(full_path), "%s/%s.%s", TEXTURE_DIRECTORY, name, extensions[i]);
        if (os_file_exists(full_path)) {
            full_path_exists = true;
            break;
        }
    }

    if (!full_path_exists) {
        logprintf("No file '%s' found in '%s'!\n", name, TEXTURE_DIRECTORY);
        return NULL;
    }
    
    Texture_Load_Job *job = new Texture_Load_Job();
    job->entry = texture_lookup.find(name);
    memcpy(job->filepath, full_path, sizeof(full_path));

    enqueue_job(texture_load_job, job);

    return NULL;
}

void Texture_Catalog::update() {
    for (int i = 0; i < texture_lookup.allocated; i++) {
        if (!texture_lookup.occupancy_mask[i]) continue;

        Texture_Entry *entry = &texture_lookup.buckets[i].value;

        if (entry->state == TEXTURE_STATE_CPU_READY) {
            entry->texture = make_texture(entry->cpu.width, entry->cpu.height, entry->cpu.format, entry->cpu.data, entry->filepath);
            stbi_image_free(entry->cpu.data);
            entry->state = TEXTURE_STATE_READY;
        }
    }
}
