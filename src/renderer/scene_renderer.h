#pragma once

#include "render_backend.h"
#include "../camera.h"

struct Mesh;
struct Submesh;
struct Terrain_Chunk;

struct Renderer_2D;

const int ATMOSPHERE_CUBEMAP_SIZE = 128;

struct Render_Entity {
    glm::mat4 world_matrix;
    Mesh *mesh;
    glm::vec4 scale_color;
};

struct Scene_Renderer {
    static const int MAX_COLOR_TARGETS   = 4;
    static const int MAX_RESOLVE_PASSES  = 7;
    static const int MAX_RENDER_PASSES   = MAX_SHADOW_CASCADES + 1;
    static const int MAX_RENDER_ENTITIES = 1024;
    
    bool init(Render_Backend *backend, Renderer_2D *renderer_2d);

    void destroy_framebuffers();
    bool init_framebuffers();

    void render();

    void set_camera(Camera camera);
    void add_light(Light light);
    void add_render_entity(Mesh *mesh, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 scale_color);
    void add_terrain_chunk(Terrain_Chunk *chunk);

private:
    void generate_gpu_data_for_submesh(Submesh *submesh);

    void render_all_entities(VkCommandBuffer cb, int cascade_index = -1);
    void render_atmosphere_cubemap(VkCommandBuffer cb);
    
    void begin_rendering(VkCommandBuffer cb, int num_color_targets, Texture *color_targets, Texture *depth_target, VkExtent2D extent, glm::vec4 *clear_colors, float z, u8 stencil,  int start_layer_index = 0);
    void end_rendering(VkCommandBuffer cb);

    void update_shadow_map_cascade_matrices(Per_Scene_Uniforms *uniforms, Light *directional_light);

    void draw_imgui_stuff();
    void draw_hud(VkExtent2D extent);

    void draw_mesh(VkCommandBuffer cb, Mesh *mesh, glm::mat4 const &world_matrix, glm::vec4 scale_color, int cascade_index = -1, Terrain_Chunk *chunk = NULL);
    void draw_mesh_instanced(VkCommandBuffer cb, Mesh *mesh, Gpu_Buffer *instance_buffer, int offset, int count, Terrain_Chunk *chunk = NULL);
    void draw_fullscreen_quad(VkCommandBuffer cb, VkPipeline pipeline, VkDescriptorSet descriptor_set, int per_scene_uds = 1);
    void draw_unit_cube(VkCommandBuffer cb, VkPipeline pipeline, VkDescriptorSet descriptor_set, int per_scene_uds_index = 1);
    
    bool create_per_scene_vulkan_objects();
    bool create_material_vulkan_objects();
    bool create_fullscreen_quad_vulkan_objects();
    bool create_pipeline_layouts();
    bool create_pipelines();
    
private:
    Render_Backend *backend = NULL;
    Renderer_2D *renderer_2d = NULL;
    
    Texture offscreen_buffer;
    Texture depth_buffer;
    Texture shadow_map_buffers[MAX_SHADOW_CASCADES];
    Texture atmosphere_buffer;
    
    float shadow_cascade_splits[MAX_SHADOW_CASCADES] = { 64.0f, 131.0f, 221.0f, 508.0f };
    
    eastl::vector <Render_Entity> render_entities;
    eastl::vector <Terrain_Chunk> terrain_chunks;
    Light lights[MAX_LIGHTS] = {};
    int num_lights = 0;
    Camera camera;
    Frustum camera_frustum;
    
    VkDescriptorPool per_scene_uniforms_descriptor_pool;
    VkDescriptorSetLayout per_scene_uniforms_descriptor_set_layout;
    VkDescriptorSet per_scene_uniforms_descriptor_sets[7 * Render_Backend::MAX_FRAMES_IN_FLIGHT];
    Gpu_Buffer per_scene_uniform_buffers[7 * Render_Backend::MAX_FRAMES_IN_FLIGHT];
    
    VkDescriptorPool material_uniforms_descriptor_pool;
    VkDescriptorSetLayout material_uniforms_descriptor_set_layout;

    VkPipelineLayout mesh_pipeline_layout;
    VkPipelineLayout mesh_instanced_pipeline_layout;
    VkPipeline mesh_pipeline;
    VkPipeline terrain_pipeline;
    VkPipeline mesh_instanced_pipeline;
    VkPipeline shadow_pipeline;
    VkPipeline shadow_instanced_pipeline;

    VkDescriptorPool fullscreen_quad_descriptor_pool;
    VkDescriptorSetLayout fullscreen_quad_descriptor_set_layout;
    VkDescriptorSet fullscreen_quad_descriptor_sets[MAX_RESOLVE_PASSES * Render_Backend::MAX_FRAMES_IN_FLIGHT];
    VkPipelineLayout fullscreen_quad_pipeline_layout;
    Gpu_Buffer fullscreen_quad_vertex_buffer;
    Gpu_Buffer fullscreen_quad_index_buffer;
    VkPipeline resolve_pipeline;
    VkPipeline atmosphere_pipeline;
    VkPipeline sky_pipeline;

    Gpu_Buffer unit_cube_vertex_buffer;
    Gpu_Buffer unit_cube_index_buffer;
    
    // Per-frame data:
    enum Render_Stage {
        RENDER_STAGE_SHADOWS,
        RENDER_STAGE_MAIN,
    };

    Render_Stage current_render_stage;
    VkPipelineLayout pipeline_layout_for_current_pass;
    VkPipeline pipeline_for_current_pass;
};
