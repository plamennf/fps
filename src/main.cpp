#include "pch.h"
#include "main.h"
#include "renderer/render_backend.h"
#include "renderer/renderer_2d.h"
#include "renderer/scene_renderer.h"
#include "renderer/texture_registry.h"
#include "renderer/mesh_registry.h"
#include "terrain.h"

#include <SDL.h>
#include <imgui_impl_sdl2.h>

Global_Variables globals;

//static glm::vec3 directional_light_direction = glm::vec3(0, -1, 0); // Noon
static glm::vec3 directional_light_direction = glm::vec3(-0.5f, -0.2f, 0); // Early morning

struct Key_State {
    bool is_down;
    bool was_down;
    bool changed;
};

static Key_State key_states[SDL_NUM_SCANCODES];

static void toggle_fullscreen(SDL_Window *window);

bool is_key_down(int key_code) {
    return key_states[key_code].is_down;
}

bool is_key_pressed(int key_code) {
    return key_states[key_code].is_down && key_states[key_code].changed;
}

bool was_key_just_released(int key_code) {
    return key_states[key_code].was_down && !key_states[key_code].is_down;
}

double nanoseconds_to_seconds(u64 nanoseconds) {
    double result = (double)nanoseconds / NS_PER_SECOND;
    return result;
}

u64 seconds_to_nanoseconds(double seconds) {
    u64 result = (u64)(seconds * NS_PER_SECOND);
    return result;
}

static void update_time() {
    s64 now_time = get_time_nanoseconds();
    globals.time_info.delta_time       = now_time - globals.time_info.last_time;
    globals.time_info.real_world_time += globals.time_info.delta_time;
    globals.time_info.delta_time_seconds = nanoseconds_to_seconds(globals.time_info.delta_time);
    globals.time_info.last_time = now_time;

    globals.time_info.num_frames_since_last_fps_update++;
    globals.time_info.accumulated_fps_dt += globals.time_info.delta_time_seconds;
    if (globals.time_info.accumulated_fps_dt >= 1.0) {
        globals.time_info.fps_dt = 1.0 / (double)globals.time_info.num_frames_since_last_fps_update;
        globals.time_info.num_frames_since_last_fps_update = 0;
        globals.time_info.accumulated_fps_dt = 0.0;
    }
}

static void toggle_fullscreen(SDL_Window *window) {
    Uint32 flags = SDL_GetWindowFlags(window);
    bool is_fullscreen = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;

    if (is_fullscreen) {
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowBordered(window, SDL_TRUE);
        SDL_SetWindowResizable(window, SDL_TRUE);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    } else {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_SetWindowBordered(window, SDL_FALSE);
    }

    SDL_GetWindowSize(window, &globals.window_width, &globals.window_height);
    //init_framebuffer();
}

static void init_window_size(int *width, int *height) {
    if (*width == -1 && *height == -1) {
        SDL_Rect usable;
        if (SDL_GetDisplayUsableBounds(0, &usable) == 0) {
            int monitor_width  = usable.w;
            int monitor_height = usable.h;

            *width  = (int)(monitor_width  * 2.0 / 3.0);
            *height = (int)(*width * (9.0 / 16.0));
        } else {
            // Fallback if SDL fails
            *width  = 1280;
            *height = 720;
        }
    } else if (*width == -1 && *height > 0) {
        *width = (int)(*height * (16.0 / 9.0));
    } else if (*width > 0 && *height == -1) {
        *height = (int)(*width * (9.0 / 16.0));
    }
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    void enable_dpi_awareness();
    enable_dpi_awareness();
#endif
        
    init_log();
    defer { close_log(); };

    bool start_fullscreen = false;

#if defined(BUILD_RELEASE) || defined(BUILD_DIST)
    start_fullscreen = true;
#endif

    start_fullscreen = true;
    
    globals.window_width  = -1;
    globals.window_height = -1;
    
    for (int i = 1; i < argc; i++) {
        const char *arg = (const char *)argv[i];
        if (strings_match(arg, "-width") || strings_match(arg, "-w")) {
            if (i == argc - 1) {
                logprintf("Tried to set window width but with no width provided!\n");
                break;
            } else {
                globals.window_width = atoi(argv[++i]);
            }
        } else if (strings_match(arg, "-height") || strings_match(arg, "-h")) {
            if (i == argc - 1) {
                logprintf("Tried to set window height but with no height provided!\n");
                break;
            } else {
                globals.window_height = atoi(argv[++i]);
            }            
        } else if (strings_match(arg, "-fullscreen") || strings_match(arg, "-f")) {
            start_fullscreen = true;
        } else if (strings_match(arg, "-windowed")) {
            start_fullscreen = false;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        logprintf("Failed to initialize SDL!\n");
        return 1;
    }
    defer { SDL_Quit(); };

    srand((u32)get_time_nanoseconds());

    init_window_size(&globals.window_width, &globals.window_height);
    globals.window = SDL_CreateWindow("First-Person Shooter!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, globals.window_width, globals.window_height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!globals.window) {
        logprintf("Failed to create window!\n");
        return 1;
    }

    globals.render_backend = new Render_Backend();
    if (!globals.render_backend->init(globals.window)) {
        return 1;
    }
    defer { globals.render_backend->device_wait_idle(); };

    globals.render_backend->imgui_init();

    globals.renderer_2d = new Renderer_2D();
    if (!globals.renderer_2d->init(globals.render_backend)) {
        return 1;
    }
    
    globals.scene_renderer = new Scene_Renderer();
    if (!globals.scene_renderer->init(globals.render_backend, globals.renderer_2d)) {
        return 1;
    }
    
    globals.texture_registry = new Texture_Registry();
    globals.mesh_registry    = new Mesh_Registry();

    u8 white_texture_data[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    globals.white_texture = new Texture();
    if (!globals.render_backend->create_texture(globals.white_texture, 1, 1, VK_FORMAT_R8G8B8A8_SRGB, white_texture_data, "white_texture")) return 1;
    
    Mesh *building = globals.mesh_registry->find_or_load("BirchTree_5");
    if (!building) return 1;

    Mesh *cube = globals.mesh_registry->find_or_load("RockPath_Square_Wide");
    if (!cube) return 1;

    Mesh *mesh = globals.mesh_registry->find_or_load("MapleTree_5");
    if (!mesh) return 1;

    SDL_SetRelativeMouseMode(SDL_TRUE);

    if (start_fullscreen) {
        toggle_fullscreen(globals.window);
    }

    Terrain_Chunk chunk;
    chunk.generate(128, 1.0f, glm::vec3(0.0f), 10000);
    
    float accumulated_dt = 0.0f;
    float fixed_update_dt = 1.0f / 60.0f;
    init_camera(&globals.camera, glm::vec3(0, 2, 0), 0, 0, 0);
    while (!globals.should_quit) {
        globals.frame_memory.reset();
        update_time();

        for (int i = 0; i < ArrayCount(key_states); i++) {
            Key_State *state = &key_states[i];
            state->was_down  = state->is_down;
            state->changed   = false;
        }

        globals.mouse_cursor_x_delta = 0.0f;
        globals.mouse_cursor_y_delta = 0.0f;
        
        bool was_resized = false;
        
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            
            switch (event.type) {
                case SDL_QUIT: {
                    globals.should_quit = true;
                } break;

                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    bool is_down = event.type == SDL_KEYDOWN;
                
                    Key_State *state = &key_states[event.key.keysym.scancode];
                    state->changed   = state->is_down != is_down;
                    state->is_down   = is_down;

                    if (is_down && !event.key.repeat) {
                        if (event.key.keysym.scancode == SDL_SCANCODE_F11) {
                            toggle_fullscreen(globals.window);
                            was_resized = true;
                        }
                    }
                } break;
                
                case SDL_WINDOWEVENT: {
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                        case SDL_WINDOWEVENT_SIZE_CHANGED: {
                            globals.window_width  = event.window.data1;
                            globals.window_height = event.window.data2;
                            was_resized = true;
                        } break;
                    }
                }

                case SDL_MOUSEMOTION: {
                    globals.mouse_cursor_x_delta = (float)event.motion.xrel;
                    globals.mouse_cursor_y_delta = -(float)event.motion.yrel;
                } break;

                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP: {
                    bool is_down = event.type == SDL_MOUSEBUTTONDOWN;

                    Key_State *state = &key_states[event.button.button];
                    state->changed   = state->is_down != is_down;
                    state->is_down   = is_down;
                } break;
            }
        }

        if (is_key_pressed(SDL_SCANCODE_ESCAPE)) {
            globals.should_show_cursor = !globals.should_show_cursor;
        }

        if (is_key_pressed(SDL_BUTTON_RIGHT)) {
            globals.flashlight_on = !globals.flashlight_on;
        }
        
        if (!globals.should_show_cursor) {
            SDL_SetRelativeMouseMode(SDL_TRUE);
        } else {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }

        float dt = (float)globals.time_info.delta_time_seconds;
        accumulated_dt += dt;
        
        if (!globals.should_show_cursor) {
            update_camera(&globals.camera, CAMERA_TYPE_FPS, dt);
        }
        
        while (accumulated_dt >= fixed_update_dt) {
            if (!globals.should_show_cursor) {
                fixed_update_camera(&globals.camera, CAMERA_TYPE_FPS, fixed_update_dt, &chunk);
            }
            accumulated_dt -= fixed_update_dt;
        }

        if (was_resized) {
            if (globals.window_width > 0 && globals.window_height > 0) {
                globals.render_backend->device_wait_idle();
                globals.scene_renderer->destroy_framebuffers();
                globals.render_backend->recreate_swap_chain();
                if (!globals.scene_renderer->init_framebuffers()) {
                    return 1;
                }
                globals.render_backend->device_wait_idle();
            }
        }

        if (globals.window_width > 0 && globals.window_height > 0) {
            MyZoneScopedN("Render one frame");
            
            if (!globals.render_backend->begin_frame()) {
                return 1;
            }
        
            VkCommandBuffer cb = globals.render_backend->get_current_command_buffer();
        
            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            if (vkBeginCommandBuffer(cb, &begin_info) != VK_SUCCESS) {
                logprintf("Failed to begin command buffer!\n");
                return 1;
            }

            Light sun = {};
            sun.type      = LIGHT_TYPE_DIRECTIONAL;
            sun.direction = glm::normalize_or_zero(directional_light_direction);
            sun.color     = glm::vec3(1.0f, 0.95f, 0.85f);
            sun.intensity = 1.2f;
            //sun.intensity = 10.0f;
    
            Light l0 = {};
            l0.type = LIGHT_TYPE_POINT;
            l0.position = glm::vec3(0.0f, 32.0f, 0.0f);
            l0.color = glm::vec3(1.0f, 0.85f, 0.7f);
            l0.intensity = 8.0f;
            l0.range = 60.0f;

            Light l1 = {};
            l1.type = LIGHT_TYPE_POINT;
            l1.position = glm::vec3(-30.0f, 15.0f, 0.0f);
            l1.color = glm::vec3(0.4f, 0.6f, 1.0f);
            l1.intensity = 4.0f;
            l1.range = 45.0f;

            Light l2 = {};
            l2.type = LIGHT_TYPE_POINT;
            l2.position = glm::vec3(30.0f, 12.0f, -10.0f);
            l2.color = glm::vec3(1.0f, 1.0f, 1.0f);
            l2.intensity = 10.0f;
            l2.range = 40.0f;

            Light l3 = {};
            l3.type = LIGHT_TYPE_POINT;
            l3.position = glm::vec3(0.0f, 10.0f, -25.0f);
            l3.color = glm::vec3(1.0f, 0.95f, 0.8f);
            l3.intensity = 3.0f;
            l3.range = 50.0f;

            Light l4 = {};
            l4.type = LIGHT_TYPE_POINT;
            l4.position = glm::vec3(-15.0f, 10.0f, 20.0f);
            l4.color = glm::vec3(0.6f, 0.7f, 1.0f);
            l4.intensity = 4.0f;
            l4.range = 35.0f;

            Light l5 = {};
            l5.type = LIGHT_TYPE_POINT;
            l5.position = glm::vec3(20.0f, 18.0f, 10.0f);
            l5.color = glm::vec3(1.0f, 0.9f, 0.75f);
            l5.intensity = 5.0f;
            l5.range = 40.0f;

            Light spot_light = {};
            if (globals.flashlight_on) {
                spot_light.type      = LIGHT_TYPE_SPOT;
                spot_light.position  = globals.camera.position;
                spot_light.direction = glm::normalize_or_zero(globals.camera.target);
                spot_light.color     = glm::vec3(1.0f, 1.0f, 0.9f);
                spot_light.intensity = 800.0f;
                spot_light.range     = 50.0f;
                spot_light.spot_inner_cone_angle = cosf(glm::radians(12.5f));
                spot_light.spot_outer_cone_angle = cosf(glm::radians(20.0f));
            }

            globals.scene_renderer->set_camera(globals.camera);
            
            globals.scene_renderer->add_light(sun);
            globals.scene_renderer->add_light(l0);
            globals.scene_renderer->add_light(l1);
            globals.scene_renderer->add_light(l2);
            globals.scene_renderer->add_light(l3);
            globals.scene_renderer->add_light(l4);
            globals.scene_renderer->add_light(l5);
            globals.scene_renderer->add_light(spot_light);

            globals.scene_renderer->add_terrain_chunk(&chunk);
            
            //globals.scene_renderer->add_render_entity(cube, {-50, -1, -50}, {0, 0, 0}, {100, 1, 100}, {1, 1, 1, 1});

            float scale = 10.0f;
            globals.scene_renderer->add_render_entity(building, {0, 0.1f, -10}, {0, 0, 0}, glm::vec3(scale), glm::vec4(1));

            scale = 1.0f;
            globals.scene_renderer->add_render_entity(mesh, {0, 0, 10}, {0, 0, 0}, glm::vec3(scale), glm::vec4(1));
        
            globals.scene_renderer->render();
            globals.renderer_2d->end_frame();
            
            if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
                logprintf("Failed to record command buffer!\n");
                return 1;
            }

            if (!globals.render_backend->end_frame()) {
                //return 1;
            }
        }

        MyFrameMark;
    }
    
    return 0;
}
