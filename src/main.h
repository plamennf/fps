#pragma once

struct SDL_Window;
struct Render_Backend;
struct Scene_Renderer;
struct Texture;

struct Texture_Registry;
struct Mesh_Registry;

struct Time_Info {
    s64 last_time = 0;
    s64 sync_last_time = 0;

    s64 real_world_time = 0;
    s64 delta_time = 0;
    double delta_time_seconds = 0.0;

    // For fps debug info.
    s64 num_frames_since_last_fps_update = 0;
    double accumulated_fps_dt = 0.0;
    double fps_dt = 0.0;
};

struct Global_Variables {
    SDL_Window *window = NULL;
    bool should_quit = false;

    int window_width  = 0;
    int window_height = 0;

    Time_Info time_info;
    Memory_Arena frame_memory;

    Render_Backend *render_backend = NULL;
    Scene_Renderer *scene_renderer = NULL;
    
    Texture_Registry *texture_registry = NULL;
    Mesh_Registry *mesh_registry = NULL;
    
    Texture *white_texture = NULL;
};

extern Global_Variables globals;

bool is_key_down(int key_code);
bool is_key_pressed(int key_code);
bool was_key_just_released(int key_code);

double nanoseconds_to_seconds(u64 nanoseconds);
u64 seconds_to_nanoseconds(double seconds);
