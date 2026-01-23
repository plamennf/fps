#include "main.h"
#include "camera.h"

#include <tracy/Tracy.hpp>

void init_camera(Camera *camera, Vector3 position, float pitch, float yaw, float roll) {
    camera->position = position;
    camera->target   = position - v3(0, 0, 1);
    camera->up       = v3(0, 1, 0);
    camera->pitch    = pitch;
    camera->yaw      = yaw;
    camera->roll     = roll;
    camera->is_on_ground = true;
}

void update_camera_fps(Camera *camera) {
    float sensitivity = globals.mouse_sensitivity;
    float dt = globals.time_info.dt;
    
    camera->yaw   += globals.mouse_x_delta * sensitivity;
    camera->pitch += globals.mouse_y_delta * sensitivity;

    if (camera->pitch > 89.0f) {
        camera->pitch = 89.0f;
    } else if (camera->pitch < -89.0f) {
        camera->pitch = -89.0f;
    }

    float old_y = camera->position.y;

    float movement_speed = 5.0f;
    if (is_key_down(SDL_SCANCODE_LSHIFT)) {
        movement_speed *= 3.0f;
    }

    Vector3 world_up = v3(0, 1, 0);
    Vector3 right = normalize_or_zero(cross_product(camera->target, world_up));
    Vector3 up = normalize_or_zero(cross_product(right, camera->target));

    camera->target = v3(0, 0, 0);
    camera->target.x = cosf(to_radians(camera->yaw)) * cosf(to_radians(camera->pitch));
    //camera->target.y = sinf(to_radians(camera->pitch));
    camera->target.z = sinf(to_radians(camera->yaw)) * cosf(to_radians(camera->pitch));
    Vector3 camera_target = normalize_or_zero(camera->target);

    if (is_key_down(SDL_SCANCODE_W)) {
        camera->position += camera_target * movement_speed * dt;
    }

    if (is_key_down(SDL_SCANCODE_S)) {
        camera->position -= camera_target * movement_speed * dt;
    }

    if (is_key_down(SDL_SCANCODE_A)) {
        camera->position -= right * movement_speed * dt;
    }

    if (is_key_down(SDL_SCANCODE_D)) {
        camera->position += right * movement_speed * dt;
    }

    camera->position.y = old_y;

    camera->target.y = sinf(to_radians(camera->pitch));
    camera->target   = normalize_or_zero(camera->target);
}

void fixed_update_camera_fps(Camera *camera) {
    float dt = globals.time_info.fixed_update_dt;
    
    if (is_key_down(SDL_SCANCODE_SPACE)) {
        if (camera->is_on_ground) {
            camera->jump_velocity = 0.5f;
            camera->is_on_ground  = false;
        }
    }

    camera->jump_velocity -= 1.0f * dt;

    camera->position.y += camera->jump_velocity;

    if (camera->position.y < 2.0f) {
        camera->position.y = 2.0f;
        camera->is_on_ground = true;
    }
}

void update_camera_noclip(Camera *camera) {
    float sensitivity = globals.mouse_sensitivity;
    float dt = globals.time_info.dt;
    
    camera->yaw   += globals.mouse_x_delta * sensitivity;
    camera->pitch += globals.mouse_y_delta * sensitivity;

    if (camera->pitch > 89.0f) {
        camera->pitch = 89.0f;
    } else if (camera->pitch < -89.0f) {
        camera->pitch = -89.0f;
    }

    float movement_speed = 5.0f;
    if (is_key_down(SDL_SCANCODE_LSHIFT)) {
        movement_speed *= 3.0f;
    }

    Vector3 world_up = v3(0, 1, 0);
    Vector3 right = normalize_or_zero(cross_product(camera->target, world_up));
    Vector3 up = normalize_or_zero(cross_product(right, camera->target));

    camera->target = v3(0, 0, 0);
    camera->target.x = cosf(to_radians(camera->yaw)) * cosf(to_radians(camera->pitch));
    camera->target.y = sinf(to_radians(camera->pitch));
    camera->target.z = sinf(to_radians(camera->yaw)) * cosf(to_radians(camera->pitch));
    Vector3 camera_target = normalize_or_zero(camera->target);

    if (is_key_down(SDL_SCANCODE_W)) {
        camera->position += camera_target * movement_speed * dt;
    }

    if (is_key_down(SDL_SCANCODE_S)) {
        camera->position -= camera_target * movement_speed * dt;
    }

    if (is_key_down(SDL_SCANCODE_A)) {
        camera->position -= right * movement_speed * dt;
    }

    if (is_key_down(SDL_SCANCODE_D)) {
        camera->position += right * movement_speed * dt;
    }

    camera->target = camera_target;
}

void update_camera(Camera *camera, Camera_Type type) {
    ZoneScoped;
    
    switch (type) {
        case CAMERA_TYPE_FPS: {
            update_camera_fps(camera);
        } break;

        case CAMERA_TYPE_NOCLIP: {
            update_camera_noclip(camera);
        } break;
    }
}

void fixed_update_camera(Camera *camera, Camera_Type type) {
    ZoneScoped;
    
    switch (type) {
        case CAMERA_TYPE_FPS: {
            fixed_update_camera_fps(camera);
        } break;
    }
}

Matrix4 get_view_matrix(Camera *camera) {
    return make_look_at_matrix(camera->position, camera->position + camera->target, camera->up);
}
