#include "main.h"
#include "renderer.h"
#include "font.h"
#include "mesh.h"
#include "mesh_catalog.h"

#include <SDL3/SDL_main.h>
#include <GL/glew.h>
#include <stdio.h>

Global_Variables globals = {};

struct Key_State {
    bool is_down;
    bool was_down;
    bool changed;
};

static Key_State key_states[SDL_SCANCODE_COUNT];
static Key_State mouse_buttons[6];

static void init_framebuffers() {    
    if (globals.offscreen_buffer) {
        destroy_framebuffer(globals.offscreen_buffer);
        globals.offscreen_buffer = NULL;
    }
    
    globals.offscreen_buffer = make_framebuffer(globals.window_width, globals.window_height, TEXTURE_FORMAT_RGBA8, TEXTURE_FORMAT_D24S8);
}

static void init_shaders() {
    globals.shader_color        = load_shader("data/shaders/color.glsl", RENDER_VERTEX_IMMEDIATE);
    globals.shader_texture      = load_shader("data/shaders/texture.glsl", RENDER_VERTEX_IMMEDIATE);
    globals.shader_basic        = load_shader("data/shaders/basic.glsl", RENDER_VERTEX_MESH);
    globals.shader_depth        = load_shader("data/shaders/depth.glsl", RENDER_VERTEX_MESH);
    globals.shader_depth_debug  = load_shader("data/shaders/depth_debug.glsl", RENDER_VERTEX_IMMEDIATE);
    globals.shader_resolve      = load_shader("data/shaders/resolve.glsl", RENDER_VERTEX_IMMEDIATE);
    globals.shader_resolve_msaa = load_shader("data/shaders/resolve_msaa.glsl", RENDER_VERTEX_IMMEDIATE);
}

static void init_lights() {
    globals.directional_light.direction = v3(-0.2f, -1.0f, -0.3f);
    globals.directional_light.ambient   = v3(0.05f, 0.05f, 0.05f);
    globals.directional_light.diffuse   = v3(0.4f, 0.4f, 0.4f);
    globals.directional_light.specular  = v3(0.5f, 0.5f, 0.5f);
    
    globals.point_lights[0].position  = v3(0.0f, 10.0f, 0.0f);
    globals.point_lights[0].ambient   = v3(0.05f, 0.05f, 0.05f);
    globals.point_lights[0].diffuse   = v3(1.0f, 0.9f, 0.7f);
    globals.point_lights[0].specular  = v3(1.0f, 1.0f, 1.0f);
    globals.point_lights[0].constant  = 1.0f;
    globals.point_lights[0].linear    = 0.14f;
    globals.point_lights[0].quadratic = 0.07f;
    
    globals.spot_light    = {};
    if (globals.flashlight_on) {
        globals.spot_light.position      = globals.camera.position;
        globals.spot_light.direction     = globals.camera.target;
        globals.spot_light.cut_off       = cosf(to_radians(12.5f));
        globals.spot_light.outer_cut_off = cosf(to_radians(17.5f));
        globals.spot_light.ambient       = v3(0.05f, 0.05f, 0.05f);
        globals.spot_light.diffuse       = v3(0.8f, 0.8f, 0.8f);
        globals.spot_light.specular      = v3(1, 1, 1);
        globals.spot_light.constant      = 1.0f;
        globals.spot_light.linear        = 0.09f;
        globals.spot_light.quadratic     = 0.032f;
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
}

static void resolve_to_screen() {
    set_framebuffer(NULL, true, v4(0, 0, 0, 1), false, 1.0f, false, 0);
    rendering_2d();

    set_shader(globals.shader_texture);
    set_color_texture(globals.offscreen_buffer);
    
    immediate_begin();
    immediate_quad(v2(0, 0), v2((float)globals.window_width, (float)globals.window_height), v4(1, 1, 1, 1));
    immediate_flush();
}

static void draw_scene() {
    draw_cube(v3(0, -1, 0), v3(0, 0, 0), v3(200, 2.0f, 200.0f), v4(0, 1, 0, 1));
    draw_cube(v3(1, 1, -25), v3(0, 0, 0), v3(2, 2, 2), v4(0, 0, 1, 1));
    draw_mesh(globals.mesh, v3(0, 0, -50), v3(0, 0, 0), 1.0f);
    //draw_mesh(globals.mesh, v3(0, 0, -50), v3(0, 0, 0), 0.01f);
}

static void draw_one_frame() {
    // Reset the depth write before clearing as set_depth_write(false) before 2D drawing
    // disables the depth write, which in turn means the depth clear won't work
    set_depth_write(true);
    
    // Normal 3D Drawing:
    set_framebuffer(globals.offscreen_buffer, true, v4(0.2f, 0.5f, 0.8f, 1.0f), true, 1.0f, false, 0);
    rendering_3d();
    draw_scene();
    
    // 2D Drawing:
    set_depth_write(false);
    draw_hud();
    
    resolve_to_screen();
}

static void toggle_fullscreen() {
    globals.is_fullscreen = !globals.is_fullscreen;
    SDL_SetWindowFullscreen(globals.window, globals.is_fullscreen);
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
    
    globals.mesh = globals.mesh_catalog->find_or_load("Yeti");
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

        if (globals.should_show_cursor) {
            SDL_SetWindowRelativeMouseMode(globals.window, false);
        } else {
            SDL_SetWindowRelativeMouseMode(globals.window, true);
        }

        if (!globals.should_show_cursor) {
            update_camera(&globals.camera);

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
