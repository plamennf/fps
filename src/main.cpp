#include "main.h"
#include "renderer.h"
#include "font.h"
#include "mesh.h"
#include "mesh_catalog.h"

#include <SDL3/SDL_main.h>
#include <GL/glew.h>
#include <stdio.h>
#include <float.h>
#include <limits.h>

Global_Variables globals = {};

struct Key_State {
    bool is_down;
    bool was_down;
    bool changed;
};

static Key_State key_states[SDL_SCANCODE_COUNT];
static Key_State mouse_buttons[6];

int get_num_multisamples(Antialiasing_Type type) {
    switch (type) {
        case ANTIALIASING_MSAA_8X: return 8;
        case ANTIALIASING_MSAA_4X: return 4;
        case ANTIALIASING_MSAA_2X: return 2;
    }

    return 0;
}

static void init_framebuffers() {    
    if (globals.offscreen_buffer) {
        destroy_framebuffer(globals.offscreen_buffer);
        globals.offscreen_buffer = NULL;
    }

    for (int i = 0; i < NUM_SHADOW_MAP_CASCADES; i++) {    
        if (!globals.shadow_map_cascade_buffers[i]) {
            globals.shadow_map_cascade_buffers[i] = make_framebuffer(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, TEXTURE_FORMAT_UNKNOWN, TEXTURE_FORMAT_SHADOW_MAP);
        }
    }

    if (globals.antialiasing_type == ANTIALIASING_MSAA_2X ||
        globals.antialiasing_type == ANTIALIASING_MSAA_4X ||
        globals.antialiasing_type == ANTIALIASING_MSAA_8X) {
        globals.offscreen_buffer = make_multisampled_framebuffer(globals.window_width, globals.window_height, TEXTURE_FORMAT_RGBA8, TEXTURE_FORMAT_D24S8, get_num_multisamples(globals.antialiasing_type));
    } else {
        globals.offscreen_buffer = make_framebuffer(globals.window_width, globals.window_height, TEXTURE_FORMAT_RGBA8, TEXTURE_FORMAT_D24S8);
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

static void init_shaders() {
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
    
    set_color_texture(globals.offscreen_buffer);
    
    immediate_begin();
    immediate_quad(v2(0, 0), v2((float)globals.window_width, (float)globals.window_height), v4(1, 1, 1, 1));
    immediate_flush();
}

static void draw_scene() {
    draw_cube(v3(0, -1, 0), v3(0, 0, 0), v3(200, 2.0f, 200.0f), v4(0, 1, 0, 1));
    draw_cube(v3(1, 1, -25), v3(0, 0, 0), v3(2, 2, 2), v4(0, 0, 1, 1));
    draw_mesh(globals.mesh, v3(0, 0, -50), v3(0, 90, 0), 1.0f);
    //draw_mesh(globals.mesh, v3(0, 0, -50), v3(0, 0, 0), 0.01f);
}

static void draw_one_frame() {
    // Reset the depth write before clearing as set_depth_write(false) before 2D drawing
    // disables the depth write, which in turn means the depth clear won't work
    set_depth_write(true);

    // Shadows Drawing:
    globals.render_stage = RENDER_STAGE_SHADOW;
    update_shadow_map_cascade_matrices();
    for (int i = 0; i < NUM_SHADOW_MAP_CASCADES; i++) {
        set_framebuffer(globals.shadow_map_cascade_buffers[i], false, v4(0, 0, 0, 0), true, 1.0f, false, 0);
        rendering_3d_shadow_map(i);
        draw_scene();
    }
    
    // Normal 3D Drawing:
    set_framebuffer(globals.offscreen_buffer, true, v4(0.2f, 0.5f, 0.8f, 1.0f), true, 1.0f, false, 0);
    globals.render_stage = RENDER_STAGE_MAIN;
    rendering_3d();
    draw_scene();
    resolve_to_screen();

    set_framebuffer(globals.offscreen_buffer, false, v4(0, 0, 0, 1), false, 1.0f, false, 0);
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

static void toggle_fullscreen() {
    globals.is_fullscreen = !globals.is_fullscreen;
    SDL_SetWindowFullscreen(globals.window, globals.is_fullscreen);
}

static void toggle_noclip() {
    if (globals.camera_type == CAMERA_TYPE_FPS) {
        globals.camera_type = CAMERA_TYPE_NOCLIP;
    } else {
        globals.camera_type = CAMERA_TYPE_FPS;
    }
}

static void update_time() {
    SDL_Time now_time;
    SDL_GetCurrentTime(&now_time);

    globals.time_info.dt_ns = now_time - globals.time_info.last_time;

    globals.time_info.last_time = now_time;

    globals.time_info.real_world_time += globals.time_info.dt_ns;
    globals.time_info.dt = (float)SDL_NS_TO_SECONDS((float)globals.time_info.dt_ns);

    globals.time_info.num_frames_since_last_fps_update++;
    globals.time_info.accumulated_fps_dt += globals.time_info.dt;
    if (globals.time_info.accumulated_fps_dt >= 1.0f) {
        globals.time_info.draw_fps_dt = 1.0f / (float)globals.time_info.num_frames_since_last_fps_update;
        globals.time_info.num_frames_since_last_fps_update = 0;
        globals.time_info.accumulated_fps_dt = 0.0f;
    }
}

int main(int argc, char *argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        logprintf("Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    defer { SDL_Quit(); };

    globals.window_width  = 1280;
    globals.window_height = 720;
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    globals.window = SDL_CreateWindow("First-Person Shooter!",
                                      globals.window_width, globals.window_height,
                                      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!globals.window) {
        logprintf("Failed to create window: %s\n", SDL_GetError());
        return false;
    }
    defer { SDL_DestroyWindow(globals.window); };
    SDL_SetWindowPosition(globals.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    globals.gl_context = SDL_GL_CreateContext(globals.window);
    if (!globals.gl_context) {
        logprintf("Failed to create OpenGL context: %s\n", SDL_GetError());
        return 1;
    }

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        logprintf("Failed to initialize GLEW: %s\n", SDL_GetError());
        return 1;
    }

#if 0
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback([](GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar* message, const void*){
        logprintf("GL: %s\n", message);
    }, nullptr);
#endif
    
    globals.should_vsync = true;
    if (globals.should_vsync) {
        SDL_GL_SetSwapInterval(1);
        logprintf("vsync: on\n");
    } else {
        SDL_GL_SetSwapInterval(0);
        logprintf("vsync: off\n");
    }

    init_renderer(globals.window);
    init_shaders();
    init_framebuffers();
    
    globals.mesh_catalog = new Mesh_Catalog();
    
    init_camera(&globals.camera, v3(0, 2, 0), 0.0f, 0.0f, 0.0f);
    
    u8 white_texture_data[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    globals.white_texture = make_texture(1, 1, TEXTURE_FORMAT_RGBA8, white_texture_data);

    u8 black_texture_data[4] = { 0x00, 0x00, 0x00, 0xFF };
    globals.black_texture = make_texture(1, 1, TEXTURE_FORMAT_RGBA8, black_texture_data);

    char *skybox_filepaths[6] = {
        "data/textures/night/right.png",
        "data/textures/night/left.png",
        "data/textures/night/top.png",
        "data/textures/night/bottom.png",
        "data/textures/night/front.png",
        "data/textures/night/back.png",
    };
    globals.skybox = load_cubemap(skybox_filepaths);
    
    globals.mesh = globals.mesh_catalog->find_or_load("Demon");
    if (!globals.mesh) return 1;
    //if (!load_mesh(globals.mesh, "data/meshes/Yeti.gltf")) return 1;
    //if (!load_mesh(globals.mesh, "data/meshes/knight.gltf")) return 1;
    //if (!load_mesh(globals.mesh, "data/meshes/knight.mesh")) return 1;
    
    toggle_fullscreen();
    
    SDL_GetCurrentTime(&globals.time_info.last_time);
    while (!globals.should_quit) {
        update_time();
        
        globals.mouse_x_delta = 0.0f;
        globals.mouse_y_delta = 0.0f;
        for (int i = 0; i < ArrayCount(key_states); i++) {
            Key_State *state = &key_states[i];
            state->was_down  = state->is_down;
            state->changed   = false;
        }

        for (int i = 0; i < ArrayCount(mouse_buttons); i++) {
            Key_State *state = &mouse_buttons[i];
            state->was_down  = state->is_down;
            state->changed   = false;
        }
        
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT: {
                    globals.should_quit = true;
                } break;

                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP: {
                    bool is_down = event.key.down;

                    Key_State *state = &key_states[event.key.scancode];
                    state->changed   = is_down != state->is_down;
                    state->is_down   = is_down;
                } break;

                case SDL_EVENT_MOUSE_MOTION: {
                    globals.mouse_x_delta = event.motion.xrel;
                    globals.mouse_y_delta = -event.motion.yrel;
                } break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP: {
                    bool is_down = event.button.down;
                    u8 button = event.button.button;
                    u8 clicks = event.button.clicks;
                    
                    Key_State *state = &mouse_buttons[button];
                    state->changed = is_down != state->is_down;
                    state->is_down = is_down;
                } break;

                case SDL_EVENT_WINDOW_RESIZED: {
                    globals.window_width  = event.window.data1;
                    globals.window_height = event.window.data2;

                    destroy_fonts();
                    init_framebuffers();
                } break;
            }
        }

        if (is_key_pressed(SDL_SCANCODE_ESCAPE)) {
            globals.should_show_cursor = !globals.should_show_cursor;
        }

        if (is_key_pressed(SDL_SCANCODE_F11)) {
            toggle_fullscreen();
        }

        if (is_key_pressed(SDL_SCANCODE_N)) {
            toggle_noclip();
        }
        
        if (globals.should_show_cursor) {
            SDL_SetWindowRelativeMouseMode(globals.window, false);
        } else {
            SDL_SetWindowRelativeMouseMode(globals.window, true);
        }

        if (!globals.should_show_cursor) {
            update_camera(&globals.camera, globals.camera_type);

            if (is_mouse_button_pressed(SDL_BUTTON_RIGHT)) {
                globals.flashlight_on = !globals.flashlight_on;
            }
        }

        init_lights();
        draw_one_frame();
        
        swap_buffers();
    }
    
    return 0;
}

bool is_key_down(SDL_Scancode scancode) {
    return key_states[scancode].is_down;
}

bool is_key_pressed(SDL_Scancode scancode) {
    return key_states[scancode].is_down && key_states[scancode].changed;
}

bool was_key_just_released(SDL_Scancode scancode) {
    return key_states[scancode].was_down && !key_states[scancode].is_down;
}

bool is_mouse_button_down(int button) {
    return mouse_buttons[button].is_down;
}

bool is_mouse_button_pressed(int button) {
    return mouse_buttons[button].is_down && mouse_buttons[button].changed;
}

bool was_mouse_button_just_released(int button) {
    return mouse_buttons[button].was_down && !mouse_buttons[button].is_down;
}
