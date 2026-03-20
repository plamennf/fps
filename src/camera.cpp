#include "pch.h"
#include "camera.h"
#include "main.h"

#include <SDL_scancode.h>

void init_camera(Camera *camera, glm::vec3 position, float pitch, float yaw, float roll) {
    camera->position = position;
    camera->target   = position - glm::vec3(0, 0, 1);
    camera->up       = glm::vec3(0, 1, 0);
    camera->pitch    = pitch;
    camera->yaw      = yaw;
    camera->roll     = roll;
    camera->is_on_ground = true;
}

void update_camera_fps(Camera *camera, float dt) {
    float sensitivity = globals.mouse_sensitivity;
    
    camera->yaw   += globals.mouse_cursor_x_delta * sensitivity;
    camera->pitch += globals.mouse_cursor_y_delta * sensitivity;

    if (camera->pitch > 89.0f) {
        camera->pitch = 89.0f;
    } else if (camera->pitch < -89.0f) {
        camera->pitch = -89.0f;
    }

    float old_y = camera->position.y;

    float movement_speed = camera->movement_speed;
    if (is_key_down(SDL_SCANCODE_LSHIFT)) {
        movement_speed *= camera->shift_movement_multiplier;
    }

    glm::vec3 world_up = glm::vec3(0, 1, 0);
    glm::vec3 right = glm::normalize_or_zero(glm::cross(camera->target, world_up));
    glm::vec3 up = glm::normalize_or_zero(glm::cross(right, camera->target));

    camera->target = glm::vec3(0, 0, 0);
    camera->target.x = cosf(glm::radians(camera->yaw)) * cosf(glm::radians(camera->pitch));
    //camera->target.y = sinf(glm::radians(camera->pitch));
    camera->target.z = sinf(glm::radians(camera->yaw)) * cosf(glm::radians(camera->pitch));
    glm::vec3 camera_target = glm::normalize_or_zero(camera->target);

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

    camera->target.y = sinf(glm::radians(camera->pitch));
    camera->target   = glm::normalize_or_zero(camera->target);
}

void fixed_update_camera_fps(Camera *camera, float dt) {
    if (is_key_down(SDL_SCANCODE_SPACE)) {
        if (camera->is_on_ground) {
            camera->jump_velocity = camera->max_jump_velocity;
            camera->is_on_ground  = false;
        }
    }

    camera->jump_velocity -= camera->gravity * dt;

    camera->position.y += camera->jump_velocity;
    
    if (camera->position.y < camera->head_y) {
        camera->position.y = camera->head_y;
        camera->is_on_ground = true;
    }
}

void update_camera_noclip(Camera *camera, float dt) {
    float sensitivity = globals.mouse_sensitivity;
    
    camera->yaw   += globals.mouse_cursor_x_delta * sensitivity;
    camera->pitch += globals.mouse_cursor_y_delta * sensitivity;

    if (camera->pitch > 89.0f) {
        camera->pitch = 89.0f;
    } else if (camera->pitch < -89.0f) {
        camera->pitch = -89.0f;
    }

    float movement_speed = camera->movement_speed;
    if (is_key_down(SDL_SCANCODE_LSHIFT)) {
        movement_speed *= camera->shift_movement_multiplier;
    }

    glm::vec3 world_up = glm::vec3(0, 1, 0);
    glm::vec3 right = glm::normalize_or_zero(glm::cross(camera->target, world_up));
    glm::vec3 up = glm::normalize_or_zero(glm::cross(right, camera->target));

    camera->target = glm::vec3(0, 0, 0);
    camera->target.x = cosf(glm::radians(camera->yaw)) * cosf(glm::radians(camera->pitch));
    camera->target.y = sinf(glm::radians(camera->pitch));
    camera->target.z = sinf(glm::radians(camera->yaw)) * cosf(glm::radians(camera->pitch));
    glm::vec3 camera_target = glm::normalize_or_zero(camera->target);

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

void update_camera(Camera *camera, Camera_Type type, float dt) {
    switch (type) {
        case CAMERA_TYPE_FPS: {
            update_camera_fps(camera, dt);
        } break;

        case CAMERA_TYPE_NOCLIP: {
            update_camera_noclip(camera, dt);
        } break;
    }
}

void fixed_update_camera(Camera *camera, Camera_Type type, float dt) {
    switch (type) {
        case CAMERA_TYPE_FPS: {
            fixed_update_camera_fps(camera, dt);
        } break;
    }
}

glm::mat4 get_view_matrix(Camera *camera) {
    return glm::lookAt(camera->position, camera->position + camera->target, camera->up);
}
