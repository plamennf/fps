#pragma once

struct Terrain_Chunk;

enum Camera_Type {
    CAMERA_TYPE_FPS,
    CAMERA_TYPE_NOCLIP,
};

struct Camera {
    glm::vec3 position;
    glm::vec3 target;
    glm::vec3 up;

    float smoothed_mouse_x = 0;
    float smoothed_mouse_y = 0;
    float smoothing_factor = 0.4f;
    
    float pitch;
    float yaw;
    float roll;

    float jump_velocity;
    bool is_on_ground;

    float z_near = 0.1f;
    float z_far = 2000.0f;
    float fov = 90.0f;
    
    float movement_speed            = 10.0f;
    float shift_movement_multiplier = 1.5f;
    float max_jump_velocity         = 0.5f;
    float gravity                   = 1.0f;
    float head_y                    = 2.0f;
};

void init_camera(Camera *camera, glm::vec3 position, float pitch, float yaw, float roll);
void update_camera(Camera *camera, Camera_Type type, float dt);
void fixed_update_camera(Camera *camera, Camera_Type type, float dt, Terrain_Chunk *chunk);
glm::mat4 get_view_matrix(Camera *camera);
glm::mat4 get_projection_matrix(Camera *camera, float aspect_ratio);
Frustum get_camera_frustum(glm::mat4 vp);
