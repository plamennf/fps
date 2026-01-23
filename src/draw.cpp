#include "draw.h"
#include "main.h"
#include "font.h"

#include <stdio.h>

static Framebuffer *offscreen_buffer = NULL;
static Framebuffer *shadow_map_cascade_buffers[NUM_SHADOW_MAP_CASCADES] = {};

void init_shaders() {
    globals.shader_color        = load_shader("data/shaders/color.glsl", RENDER_VERTEX_IMMEDIATE);
    globals.shader_texture      = load_shader("data/shaders/texture.glsl", RENDER_VERTEX_IMMEDIATE);
    globals.shader_basic        = load_shader("data/shaders/basic.glsl", RENDER_VERTEX_MESH);
    globals.shader_depth        = load_shader("data/shaders/depth.glsl", RENDER_VERTEX_MESH);
    globals.shader_depth_debug  = load_shader("data/shaders/depth_debug.glsl", RENDER_VERTEX_IMMEDIATE);
    globals.shader_resolve      = load_shader("data/shaders/resolve.glsl", RENDER_VERTEX_IMMEDIATE);
    globals.shader_resolve_msaa = load_shader("data/shaders/resolve_msaa.glsl", RENDER_VERTEX_IMMEDIATE);
    globals.shader_skybox       = load_shader("data/shaders/skybox.glsl", RENDER_VERTEX_MESH);
    globals.shader_fxaa         = load_shader("data/shaders/fxaa.glsl", RENDER_VERTEX_IMMEDIATE);
}

int get_num_multisamples(Antialiasing_Type type) {
    switch (type) {
        case ANTIALIASING_MSAA_8X: return 8;
        case ANTIALIASING_MSAA_4X: return 4;
        case ANTIALIASING_MSAA_2X: return 2;
    }

    return 0;
}

void init_framebuffers() {    
    if (offscreen_buffer) {
        destroy_framebuffer(offscreen_buffer);
        offscreen_buffer = NULL;
    }

    for (int i = 0; i < NUM_SHADOW_MAP_CASCADES; i++) {    
        if (!shadow_map_cascade_buffers[i]) {
            shadow_map_cascade_buffers[i] = make_framebuffer(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, TEXTURE_FORMAT_UNKNOWN, TEXTURE_FORMAT_SHADOW_MAP);
        }
    }

    if (globals.antialiasing_type == ANTIALIASING_MSAA_2X ||
        globals.antialiasing_type == ANTIALIASING_MSAA_4X ||
        globals.antialiasing_type == ANTIALIASING_MSAA_8X) {
        offscreen_buffer = make_multisampled_framebuffer(globals.window_width, globals.window_height, TEXTURE_FORMAT_RGBA8, TEXTURE_FORMAT_D24S8, get_num_multisamples(globals.antialiasing_type));
    } else {
        offscreen_buffer = make_framebuffer(globals.window_width, globals.window_height, TEXTURE_FORMAT_RGBA8, TEXTURE_FORMAT_D24S8);
    }
}

static void update_shadow_map_cascade_matrices() {
    for (int i = 0; i < NUM_SHADOW_MAP_CASCADES; i++) {
        float prev_split = (i == 0) ? CAMERA_Z_NEAR : globals.shadow_map_cascade_splits[i - 1];
        float curr_split = globals.shadow_map_cascade_splits[i];

        Vector3 camera_position = globals.camera.position;
        Vector3 light_direction = normalize_or_zero(globals.directional_light.direction);

        float s = curr_split;
        Matrix4 light_proj = make_orthographic(-s, s, -s, s, 0.1f, 1000.0f);

        Vector3 light_eye = camera_position - (light_direction * 500.0f);
        //Vector3 light_eye = light_direction * -500.0f;

        Vector3 world_up = (fabsf(light_direction.y) > 0.99f) ? v3(0, 0, 1) : v3(0, 1, 0);

        Matrix4 light_view = make_look_at_matrix(light_eye, camera_position, v3(0, 1, 0));
        globals.shadow_map_cascade_matrices[i] = light_proj * light_view;
    }
    
    refresh_csm();
}


static void init_lights() {
    globals.directional_light.direction = v3(-0.4f, -1.0f, -0.2f);
    globals.directional_light.ambient   = v3(0.01f, 0.02f, 0.04f);
    globals.directional_light.diffuse   = v3(0.08f, 0.12f, 0.20f);
    globals.directional_light.specular  = v3(0.15f, 0.18f, 0.25f);
    
    globals.point_lights[0].position  = v3(0.0f, 10.0f, 0.0f);
    globals.point_lights[0].ambient   = v3(0.02f, 0.015f, 0.01f);
    globals.point_lights[0].diffuse   = v3(0.8f, 0.65f, 0.45f);
    globals.point_lights[0].specular  = v3(0.9f, 0.8f, 0.7f);
    globals.point_lights[0].constant  = 1.0f;
    globals.point_lights[0].linear    = 0.22f;
    globals.point_lights[0].quadratic = 0.20f;
    
    globals.spot_light    = {};
    if (globals.flashlight_on) {
        globals.spot_light.position      = globals.camera.position;
        globals.spot_light.direction     = globals.camera.target;
        globals.spot_light.cut_off       = cosf(to_radians(10.0f));
        globals.spot_light.outer_cut_off = cosf(to_radians(15.0f));
        globals.spot_light.ambient       = v3(0.0f, 0.0f, 0.0f);
        globals.spot_light.diffuse       = v3(0.9f, 0.9f, 1.0f);
        globals.spot_light.specular      = v3(1.0f, 1.0f, 1.0f);
        globals.spot_light.constant      = 1.0f;
        globals.spot_light.linear        = 0.07f;
        globals.spot_light.quadratic     = 0.017f;
    }
}

static void draw_hud() {
    set_shader(globals.shader_texture);
    rendering_2d();
    
    int font_size = (int)(0.03f * globals.render_target_height);
    Font *font = get_font_at_size("data/fonts/OpenSans-Regular.ttf", font_size);
    char text[128];
    snprintf(text, sizeof(text), "FPS: %d", (int)(1.0f / globals.time_info.draw_fps_dt));
    int x = globals.render_target_width  - get_text_width(font, text);
    int y = globals.render_target_height - font->character_height;
    draw_text(font, text, x, y, v4(1, 1, 1, 1));

    if (globals.camera_type == CAMERA_TYPE_NOCLIP) {
        y -= font->character_height;
        snprintf(text, sizeof(text), "Noclip Enabled");
        x = globals.render_target_width - get_text_width(font, text);
        draw_text(font, text, x, y, v4(1, 1, 1, 1));
    }

    if (globals.antialiasing_type != ANTIALIASING_NONE) {
        y -= font->character_height;

        if (globals.antialiasing_type == ANTIALIASING_FXAA) {
            snprintf(text, sizeof(text), "FXAA Enabled");
        } else if (globals.antialiasing_type == ANTIALIASING_MSAA_2X) {
            snprintf(text, sizeof(text), "MSAA 2X Enabled");
        } else if (globals.antialiasing_type == ANTIALIASING_MSAA_4X) {
            snprintf(text, sizeof(text), "MSAA 4X Enabled");
        } else if (globals.antialiasing_type == ANTIALIASING_MSAA_8X) {
            snprintf(text, sizeof(text), "MSAA 8X Enabled");
        }
        
        x = globals.render_target_width - get_text_width(font, text);
        draw_text(font, text, x, y, v4(1, 1, 1, 1));
    }
}

static void resolve_to_screen(bool clear_back_buffer = true) {
    set_framebuffer(NULL, clear_back_buffer, v4(0, 0, 0, 1), false, 1.0f, false, 0);
    rendering_2d();

    if (globals.antialiasing_type == ANTIALIASING_MSAA_8X ||
        globals.antialiasing_type == ANTIALIASING_MSAA_4X ||
        globals.antialiasing_type == ANTIALIASING_MSAA_2X) {
        set_shader(globals.shader_resolve_msaa);
    } else {
        if (clear_back_buffer && globals.antialiasing_type == ANTIALIASING_FXAA) {
            set_shader(globals.shader_fxaa);
        }
        else {
            set_shader(globals.shader_texture);
        }
    }
    
    set_color_texture(offscreen_buffer);
    
    immediate_begin();
    immediate_quad(v2(0, 0), v2((float)globals.window_width, (float)globals.window_height), v4(1, 1, 1, 1));
    immediate_flush();
}

void rendering_3d_shadow_map(int cascade_index) {
    globals.transform.light_matrix = globals.shadow_map_cascade_matrices[cascade_index];

    refresh_transform();

    set_blend_mode(BLEND_MODE_OFF);
    set_depth_test(DEPTH_TEST_LESS);
    set_cull_face(CULL_FACE_FRONT);
    set_depth_write(true);

    for (int i = 0; i < NUM_SHADOW_MAP_CASCADES; i++) {
        set_shadow_map(shadow_map_cascade_buffers[i], i);
    }
}

static void draw_scene() {
    draw_cube(v3(0, -1, 0), v3(0, 0, 0), v3(200, 2.0f, 200.0f), v4(0, 1, 0, 1));
    draw_cube(v3(1, 1, -25), v3(0, 0, 0), v3(2, 2, 2), v4(0, 0, 1, 1));
    draw_mesh(globals.mesh, v3(0, 0, -50), v3(0, 90, 0), 1.0f);
    //draw_mesh(globals.mesh, v3(0, 0, -50), v3(0, 0, 0), 0.01f);
}

void draw_one_frame() {
    init_lights();
    
    // Reset the depth write before clearing as set_depth_write(false) before 2D drawing
    // disables the depth write, which in turn means the depth clear won't work
    set_depth_write(true);
    
    // Shadows Drawing:
    globals.render_stage = RENDER_STAGE_SHADOW;
    update_shadow_map_cascade_matrices();
    for (int i = 0; i < NUM_SHADOW_MAP_CASCADES; i++) {
        set_framebuffer(shadow_map_cascade_buffers[i], false, v4(0, 0, 0, 0), true, 1.0f, false, 0);
        rendering_3d_shadow_map(i);
        draw_scene();
    }
    
    // Normal 3D Drawing:
    set_framebuffer(offscreen_buffer, true, v4(0.2f, 0.5f, 0.8f, 1.0f), true, 1.0f, false, 0);
    globals.render_stage = RENDER_STAGE_MAIN;
    rendering_3d();
    draw_scene();
    resolve_to_screen();

    set_framebuffer(offscreen_buffer, false, v4(0, 0, 0, 1), false, 1.0f, false, 0);
    // Skybox Drawing:
    rendering_3d();
    set_depth_write(false);
    set_cull_face(CULL_FACE_FRONT);
    set_shader(globals.shader_skybox);
    set_cube_map(globals.skybox);
    draw_cube(v3(0, 0, 0), v3(0, 0, 0), v3(2, 2, 2), v4(1, 1, 1, 1));
    set_shader(NULL);
    
    // 2D Drawing:
    set_depth_write(false);
    set_cull_face(CULL_FACE_NONE);
    draw_hud();
    
    resolve_to_screen(false);
}
