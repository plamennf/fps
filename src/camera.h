#pragma once

enum Camera_Type {
    CAMERA_TYPE_FPS,
    CAMERA_TYPE_NOCLIP,
};

struct Camera {
    Vector3 position;
    Vector3 target;
    Vector3 up;

    float pitch;
    float yaw;
    float roll;

    float jump_velocity;
    bool is_on_ground;
};

void init_camera(Camera *camera, Vector3 position, float pitch, float yaw, float roll);
void update_camera(Camera *camera, Camera_Type type);
Matrix4 get_view_matrix(Camera *camera);
