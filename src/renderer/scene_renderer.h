#pragma once

#include "render_backend.h"
#include "../camera.h"

struct Mesh;
struct Submesh;

struct Renderer_2D;

const int MAX_LIGHTS = 8;

enum Light_Type : int {
    LIGHT_TYPE_UNKNOWN,
    LIGHT_TYPE_DIRECTIONAL,
    LIGHT_TYPE_POINT,
    LIGHT_TYPE_SPOT,
};

struct Light {
    Light_Type type;
    int _padding0[3];
    glm::vec3 position;
    float _padding1;
    glm::vec3 direction;
    float _padding2;
    glm::vec3 color;
    float intensity;
    float range;
    float spot_inner_cone_angle;
    float spot_outer_cone_angle;
    float _padding3;
};
static_assert(sizeof(Light) % 16 == 0, "Light struct must be 16-byte aligned");

struct Per_Scene_Uniforms {
    glm::mat4 projection_matrix;
    glm::mat4 view_matrix;
    Light lights[MAX_LIGHTS];
    glm::vec3 camera_position;
    float _padding0;
};

struct Per_Object_Uniforms {
    glm::mat4 world_matrix;
    glm::vec4 scale_color;
};
static_assert(sizeof(Per_Object_Uniforms) % 80 == 0, "Per_Object_Uniforms has mismatched size");

struct Material_Uniforms {
    glm::vec4 albedo_factor;
    glm::vec3 emissive_factor;
    int uses_specular_glossiness;
    int has_normal_map;
};

struct Render_Entity {
    glm::mat4 world_matrix;
    Mesh *mesh;
    glm::vec4 scale_color;
};

struct Scene_Renderer {
    static const int MAX_RENDER_ENTITIES = 1024;
    
    bool init(Render_Backend *backend, Renderer_2D *renderer_2d);

    void destroy_framebuffers();
    bool init_framebuffers();

    void render();

    void set_camera(Camera camera);
    void add_light(Light light);
    void add_render_entity(Mesh *mesh, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 scale_color);

private:
    void generate_gpu_data_for_submesh(Submesh *submesh);

    void render_all_entities(VkCommandBuffer cb);

    void begin_rendering(VkCommandBuffer cb, Texture *color_target, Texture *depth_target, VkExtent2D extent, glm::vec4 clear_color, float z, u8 stencil);
    void end_rendering(VkCommandBuffer cb);
    
private:
    Render_Backend *backend = NULL;
    Renderer_2D *renderer_2d = NULL;
    
    Texture offscreen_buffer;
    Texture depth_buffer;
    
    eastl::vector <Render_Entity> render_entities;
    Light lights[MAX_LIGHTS] = {};
    int num_lights = 0;
    Camera camera;
    
    VkDescriptorPool per_scene_uniforms_descriptor_pool;
    VkDescriptorSetLayout per_scene_uniforms_descriptor_set_layout;
    VkDescriptorSet per_scene_uniforms_descriptor_sets[Render_Backend::MAX_FRAMES_IN_FLIGHT];
    Gpu_Buffer per_scene_uniform_buffers[Render_Backend::MAX_FRAMES_IN_FLIGHT];
    
    VkDescriptorPool material_uniforms_descriptor_pool;
    VkDescriptorSetLayout material_uniforms_descriptor_set_layout;

    VkPipelineLayout mesh_pipeline_layout;
    VkPipeline mesh_pipeline;

    VkPipeline resolve_pipeline;
};
