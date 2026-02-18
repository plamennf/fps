#include <tracy/Tracy.hpp>

#include "main.h"
#include "draw.h"
#include "font.h"
#include "mesh.h"
#include "mesh_catalog.h"
#include "texture_catalog.h"
#include "entity.h"
#include "entity_manager.h"
#include "job_system.h"
#include "console.h"

#include <SDL3/SDL_main.h>
#include <GL/glew.h>
#include <stdio.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>

extern "C" 
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

//#define DO_DAY

Global_Variables globals = {};

struct Key_State {
    bool is_down;
    bool was_down;
    bool changed;
};

static Key_State key_states[SDL_SCANCODE_COUNT];
static Key_State mouse_buttons[6];

static void init_lights() {
    ZoneScopedN("Init Lights");
    
#ifdef DO_DAY
    globals.directional_light.direction = v3(-0.981f, -0.196f, 0.0f);
    globals.directional_light.ambient   = v3(0.2f, 0.1f, 0.05f);
    globals.directional_light.diffuse   = v3(0.8f, 0.5f, 0.3f);
    globals.directional_light.specular  = v3(0.5f, 0.3f, 0.3f);
    
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
#else
    globals.directional_light.direction = v3(-0.2f, -1.0f, -0.1f);
    globals.directional_light.ambient   = v3(0.002f, 0.004f, 0.008f);
    globals.directional_light.diffuse   = v3(0.05f, 0.07f, 0.10f);
    globals.directional_light.specular  = v3(0.08f, 0.10f, 0.12f);
    
    globals.point_lights[0].position  = v3(0.0f, 2.0f, -100.0f);
    globals.point_lights[0].ambient   = v3(0.0f, 0.0f, 0.0f);
    globals.point_lights[0].diffuse   = v3(0.9f, 0.6f, 0.25f);
    globals.point_lights[0].specular  = v3(1.0f, 0.8f, 0.5f);
    globals.point_lights[0].constant  = 1.0f;
    globals.point_lights[0].linear    = 0.35f;
    globals.point_lights[0].quadratic = 0.44f;
    
    globals.spot_light    = {};
    if (globals.flashlight_on) {
        globals.spot_light.position      = globals.camera.position;
        globals.spot_light.direction     = globals.camera.target;
        globals.spot_light.cut_off       = cosf(to_radians(8.0f));
        globals.spot_light.outer_cut_off = cosf(to_radians(12.0f));
        globals.spot_light.ambient       = v3(0.0f, 0.0f, 0.0f);
        globals.spot_light.diffuse       = v3(1.2f, 1.2f, 1.1f);
        globals.spot_light.specular      = v3(1.5f, 1.5f, 1.5f);
        globals.spot_light.constant      = 1.0f;
        globals.spot_light.linear        = 0.09f;
        globals.spot_light.quadratic     = 0.032f;
    }
#endif
}

static void toggle_fullscreen() {
    globals.is_fullscreen = !globals.is_fullscreen;
    SDL_SetWindowFullscreen(globals.window, globals.is_fullscreen);

    SDL_GL_SetSwapInterval(0);
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

    globals.time_info.accumulated_dt += globals.time_info.dt;
    
    globals.time_info.num_frames_since_last_fps_update++;
    globals.time_info.accumulated_fps_dt += globals.time_info.dt;
    if (globals.time_info.accumulated_fps_dt >= 1.0f) {
        globals.time_info.draw_fps_dt = 1.0f / (float)globals.time_info.num_frames_since_last_fps_update;
        globals.time_info.num_frames_since_last_fps_update = 0;
        globals.time_info.accumulated_fps_dt = 0.0f;
    }
}

static void init_test_world() {
    Entity_Manager *manager = globals.entity_manager;
    
    Entity *cube      = make_entity(manager);
    cube->position    = v3(1, 1, 0);
    cube->orientation = Quaternion();
    cube->scale       = v3(2, 2, 2);
    cube->scale_color = v4(0, 0, 1, 1);

    Entity *crypt      = make_entity(manager);
    crypt->position    = v3(0, 0.1f, -100.0f);
    set_from_axis_and_angle(&crypt->orientation, v3(0, 0, 0), 0.0f);
    crypt->scale       = v3(5);
    crypt->scale_color = v4(1, 1, 1, 1);
    set_mesh(crypt, "crypt-large");

    Entity *crypt_roof      = make_entity(manager);
    crypt_roof->position    = crypt->position + v3(0, 4.9f, 0);
    crypt_roof->scale       = v3(5);
    crypt_roof->scale_color = v4(1, 1, 1, 1);
    set_mesh(crypt_roof, "crypt-large-roof");

    // To the left of the crypt
    {
        Entity *wall      = make_entity(manager);
        wall->position    = v3(-5.0f, 0.0f, -94.0f);
        wall->scale       = v3(2);
        wall->scale_color = v4(1, 1, 1, 1);
        set_mesh(wall, "brick-wall");
    }
    for (float x = -96.5f; x < -5.0f; x += 3.0f) {
        Entity *wall      = make_entity(manager);
        wall->position    = v3(x, 0.0f, -94.0f);
        wall->scale       = v3(3, 2, 2);
        wall->scale_color = v4(1, 1, 1, 1);
        set_mesh(wall, "brick-wall");
    }
    {
        Entity *wall      = make_entity(manager);
        wall->position    = v3(-99.0f, 0.0f, -94.0f);
        wall->scale       = v3(2);
        wall->scale_color = v4(1, 1, 1, 1);
        set_from_axis_and_angle(&wall->orientation, v3(0, 1, 0), -90.0f);
        set_mesh(wall, "brick-wall-curve");
    }

    // To the right of the crypt
    {
        Entity *wall      = make_entity(manager);
        wall->position    = v3(+5.0f, 0.0f, -94.0f);
        wall->scale       = v3(2);
        wall->scale_color = v4(1, 1, 1, 1);
        set_mesh(wall, "brick-wall");
    }
    for (float x = 96.5f; x > 5.0f; x -= 3.0f) {
        Entity *wall      = make_entity(manager);
        wall->position    = v3(x, 0.0f, -94.0f);
        wall->scale       = v3(3, 2, 2);
        wall->scale_color = v4(1, 1, 1, 1);
        set_mesh(wall, "brick-wall");
    }
    {
        Entity *wall      = make_entity(manager);
        wall->position    = v3(99.0f, 0.0f, -94.0f);
        wall->scale       = v3(2);
        wall->scale_color = v4(1, 1, 1, 1);
        //set_from_axis_and_angle(&wall->orientation, v3(0, 1, 0), -90.0f);
        set_mesh(wall, "brick-wall-curve");
    }

    // Left side
    for (float z = 97.0f; z > -95.0f; z -= 3.0f) {
        Entity *wall      = make_entity(manager);
        wall->position    = v3(-99.0f, 0.0f, z);
        wall->scale       = v3(2, 2, 3);
        wall->scale_color = v4(1, 1, 1, 1);
        set_from_axis_and_angle(&wall->orientation, v3(0, 1, 0), -90.0f);
        set_mesh(wall, "brick-wall");
    }
    {
        Entity *wall      = make_entity(manager);
        wall->position    = v3(-99.0f, 0.0f, 99.0f);
        wall->scale       = v3(2);
        wall->scale_color = v4(1, 1, 1, 1);
        set_from_axis_and_angle(&wall->orientation, v3(0, 1, 0), 180.0f);
        set_mesh(wall, "brick-wall-curve");
    }

    // Right side
    for (float z = 97.0f; z > -95.0f; z -= 3.0f) {
        Entity *wall      = make_entity(manager);
        wall->position    = v3(99.0f, 0.0f, z);
        wall->scale       = v3(2, 2, 3);
        wall->scale_color = v4(1, 1, 1, 1);
        set_from_axis_and_angle(&wall->orientation, v3(0, 1, 0), 90.0f);
        set_mesh(wall, "brick-wall");
    }
    {
        Entity *wall      = make_entity(manager);
        wall->position    = v3(99.0f, 0.0f, 99.0f);
        wall->scale       = v3(2);
        wall->scale_color = v4(1, 1, 1, 1);
        set_from_axis_and_angle(&wall->orientation, v3(0, 1, 0), 90.0f);
        set_mesh(wall, "brick-wall-curve");
    }

    // Behind
    for (float x = -97.0f; x < 98.0f; x += 3.0f) {
        Entity *wall      = make_entity(manager);
        wall->position    = v3(x, 0, 99.0f);
        wall->scale       = v3(3, 2, 2);
        wall->scale_color = v4(1, 1, 1, 1);
        set_from_axis_and_angle(&wall->orientation, v3(0, 1, 0), 180.0f);
        set_mesh(wall, "brick-wall");
    }
    {
        Entity *wall      = make_entity(manager);
        wall->position    = v3(97.0f, 0.0f, 99.0f);
        wall->scale       = v3(2);
        wall->scale_color = v4(1, 1, 1, 1);
        set_from_axis_and_angle(&wall->orientation, v3(0, 1, 0), 180.0f);
        set_mesh(wall, "brick-wall");
    }
    
    Entity *demon = make_entity(manager);
    demon->position    = v3(0, 0, -20);
    set_from_axis_and_angle(&demon->orientation, v3(0, 1, 0), -90.0f);
    demon->scale       = v3(1, 1, 1);
    demon->scale_color = v4(100, 0, 0, 1);
    set_mesh(demon, "Demon");

    Entity *birb = make_entity(manager);
    birb->position    = v3(0, 0, 25);
    set_from_axis_and_angle(&birb->orientation, v3(0, 1, 0), -90.0f);
    birb->scale       = v3(1, 1, 1);
    birb->scale_color = v4(0, 0, 2000, 1);
    set_mesh(birb, "Birb");

    Entity *zombie      = make_entity(manager);
    zombie->position    = v3(20, 0, 0);
    zombie->scale       = v3(0.013f);
    zombie->scale_color = v4(1, 1, 1, 1);
    set_mesh(zombie, "Zombie");

    Quaternion zombie_zr, zombie_yr;
    set_from_axis_and_angle(&zombie_zr, v3(0, 0, 1), 90.0f);
    set_from_axis_and_angle(&zombie_yr, v3(0, 1, 0), 90.0f);
    zombie->orientation = zombie_yr * zombie_zr;

    Entity *knight = make_entity(manager);
    knight->position = v3(-25.0f, 0, -12.5f);
    set_from_axis_and_angle(&knight->orientation, v3(0, 1, 0), -60.0f);
    knight->scale = v3(0.008f);
    knight->scale_color = v4(1, 1, 1, 1);
    set_mesh(knight, "knight");
}

int main(int argc, char *argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        logprintf("Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    defer { SDL_Quit(); };

    {
        SDL_Time current_time;
        SDL_GetCurrentTime(&current_time);
        srand((u32)current_time);
    }

    init_job_system();
    defer { shutdown_job_system(); };
    
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
    
    globals.should_vsync = false;
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
    
    globals.texture_catalog = new Texture_Catalog();
    globals.mesh_catalog = new Mesh_Catalog();
    
    init_camera(&globals.camera, v3(0, 2, 0), 0.0f, 0.0f, 0.0f);
    
    u8 white_texture_data[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    globals.white_texture = make_texture(1, 1, TEXTURE_FORMAT_RGBA8, white_texture_data);

    u8 black_texture_data[4] = { 0x00, 0x00, 0x00, 0xFF };
    globals.black_texture = make_texture(1, 1, TEXTURE_FORMAT_RGBA8, black_texture_data);

#ifdef DO_DAY
    char *skybox_filepaths[6] = {
        "data/textures/skybox/right.jpg",
        "data/textures/skybox/left.jpg",
        "data/textures/skybox/top.jpg",
        "data/textures/skybox/bottom.jpg",
        "data/textures/skybox/front.jpg",
        "data/textures/skybox/back.jpg",
    };
#else
    char *skybox_filepaths[6] = {
        "data/textures/night/right.png",
        "data/textures/night/left.png",
        "data/textures/night/top.png",
        "data/textures/night/bottom.png",
        "data/textures/night/front.png",
        "data/textures/night/back.png",
    };
#endif
    globals.skybox = load_cubemap(skybox_filepaths);

    globals.entity_manager = new Entity_Manager();
    init_test_world();
        
    toggle_fullscreen();
    
    SDL_GetCurrentTime(&globals.time_info.last_time);
    while (!globals.should_quit) {
        ZoneScoped;
        
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

        if (is_key_pressed(SDL_SCANCODE_F11)) {
            toggle_fullscreen();
        }

        if (is_key_pressed(SDL_SCANCODE_N)) {
            toggle_noclip();
        }

        if (is_key_pressed(SDL_SCANCODE_F1 /*GRAVE*/)) {
            toggle_console();
        }

        if (!is_console_open()) {
            if (is_key_pressed(SDL_SCANCODE_ESCAPE)) {
                globals.should_show_cursor = !globals.should_show_cursor;
            }
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
        
        while (globals.time_info.accumulated_dt >= globals.time_info.fixed_update_dt) {
            fixed_update_camera(&globals.camera, globals.camera_type);

            globals.time_info.accumulated_dt -= globals.time_info.fixed_update_dt;
        }

        {
            ZoneScopedN("Render frame");
            init_lights();
            draw_one_frame();
            draw_console(globals.time_info.dt);
        }

        {
            ZoneScopedN("Do Entity Destruction");
            do_entity_destruction(globals.entity_manager);
        }

        {
            ZoneScopedN("Swap Buffers");
            swap_buffers();
        }

        {
            ZoneScopedN("Resource Catalog Updates");
            globals.texture_catalog->update();
            globals.mesh_catalog->update();
        }
        
        FrameMark;
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
