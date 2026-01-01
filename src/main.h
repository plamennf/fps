#pragma once

#include "general.h"
#include "geometry.h"
#include "array.h"
#include "hash_table.h"
#include "camera.h"

#include <SDL3/SDL.h>

#include "renderer.h"

const int SHADOW_MAP_WIDTH  = 4096;
const int SHADOW_MAP_HEIGHT = 4096;
const int NUM_SHADOW_MAP_CASCADES = 4;      

const float CAMERA_FOV = 90.0f;
const float CAMERA_Z_NEAR = 0.1f;
const float CAMERA_Z_FAR  = 500.0f;

struct Mesh;
struct Framebuffer;
struct Mesh_Catalog;

enum Render_Stage {
    RENDER_STAGE_SHADOW,
    RENDER_STAGE_MAIN,
};

struct Time_Info {
    s64 last_time;

    s64 real_world_time;
    s64 dt_ns;

    float dt;

    // Debug draw hud info:
    int num_frames_since_last_fps_update;
    float accumulated_fps_dt;
    float draw_fps_dt = 1.0f;
};

struct Global_Variables {
    SDL_Window *window = NULL;
    SDL_GLContext gl_context = NULL;
    bool should_vsync = true;
    bool should_quit = false;
    bool is_fullscreen = false;
    bool should_show_cursor = false;
    bool flashlight_on = false;

    Time_Info time_info = {};

    float mouse_sensitivity = 0.1f; // @PC
    //float mouse_sensitivity = 0.2f; // @Laptop
    
    float mouse_x_delta = 0;
    float mouse_y_delta = 0;

    int window_width  = 0;
    int window_height = 0;

    int render_target_width  = 0;
    int render_target_height = 0;
    
    Camera camera = {};
    
    Texture *white_texture = NULL;
    Texture *black_texture = NULL;

    Framebuffer *offscreen_buffer = NULL;
    Framebuffer *shadow_map_cascade_buffers[NUM_SHADOW_MAP_CASCADES] = {};
    Matrix4 shadow_map_cascade_matrices[NUM_SHADOW_MAP_CASCADES] = {};
    float shadow_map_cascade_splits[NUM_SHADOW_MAP_CASCADES] = { 30.0f, 100.0f, 200.0f, 400.0f };
    
    Shader *shader_color = NULL;
    Shader *shader_texture = NULL;
    Shader *shader_basic = NULL;
    Shader *shader_depth = NULL;
    Shader *shader_depth_debug = NULL;
    Shader *shader_resolve = NULL;
    Shader *shader_resolve_msaa = NULL;

    Mesh *mesh = NULL;
    
    Transform transform = {};
    Directional_Light directional_light = {};
    Point_Light point_lights[MAX_POINT_LIGHTS] = {};
    Spot_Light spot_light = {};

    Render_Stage render_stage = RENDER_STAGE_MAIN;

    Mesh_Catalog *mesh_catalog = NULL;
};

extern Global_Variables globals;

bool is_key_down(SDL_Scancode scancode);
bool is_key_pressed(SDL_Scancode scancode);
bool was_key_just_released(SDL_Scancode scancode);

bool is_mouse_button_down(int button);
bool is_mouse_button_pressed(int button);
bool was_mouse_button_just_released(int button);
