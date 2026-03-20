#include "pch.h"
#include "texture_registry.h"
#include "render_backend.h"
#include "../main.h"

#include <stdio.h>

#include <filesystem>

// TODO: Copy-paste: Move this into corelib
static bool file_exists(char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

Texture *Texture_Registry::find_or_load(char *name) {
    auto it = texture_lookup.find(name);
    if (it != texture_lookup.end()) {
        return it->second;
    }

    char *extensions[] = {
        "png",
        "jpg",
        "jpeg",
        "hdr",
        "bmp",
    };

    char full_path[256]; bool full_path_exists = false;
    for (int i = 0; i < ArrayCount(extensions); i++) {
        snprintf(full_path, sizeof(full_path), "data/textures/%s.%s", name, extensions[i]);
        if (file_exists(full_path)) {
            full_path_exists = true;
            break;
        }
    }

    if (!full_path_exists) {
        logprintf("No texture '%s' found in 'data/textures'\n", name);
        return NULL;
    }

    Texture *texture = new Texture();
    if (!globals.render_backend->load_texture(texture, full_path)) {
        delete texture;
        return NULL;
    }

    texture_lookup.insert({name, texture});

    return texture;
}

void Texture_Registry::recursive_init_all() {
    for (const auto &entry : std::filesystem::recursive_directory_iterator("data/textures")) {
        if (entry.is_regular_file()) {
            auto cpp_filename = entry.path().relative_path().string();
            char *filename = (char *)cpp_filename.c_str();

            filename = copy_string(filename);
            defer { delete [] filename; };

            char *start = find_character_from_left(filename, '\\');
            if (start) {
                start++;
            }

            char *slash = strrchr(start, '.');
            if (slash) {
                filename[slash - filename] = 0;
            }
            
            find_or_load(start);
        }
    }
}
