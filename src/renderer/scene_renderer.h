#pragma once

#include "render_backend.h"

struct Mesh;
struct Submesh;

struct Per_Scene_Uniforms {
    glm::mat4 projection_matrix;
    glm::mat4 view_matrix;
};

struct Per_Object_Uniforms {
    glm::mat4 world_matrix;
    glm::vec4 scale_color;
};

struct Render_Entity {
    glm::mat4 world_matrix;
    Mesh *mesh;
    glm::vec4 scale_color;
};

struct Scene_Renderer {
    static const int MAX_RENDER_ENTITIES = 1024;
    
    bool init(Render_Backend *backend);

    void destroy_framebuffers();
    bool init_framebuffers();

    void render();
    
    void add_render_entity(Mesh *mesh, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 scale_color);

private:
    void generate_gpu_data_for_submesh(Submesh *submesh);
    
private:
    Render_Backend *backend = NULL;
    Texture depth_buffer;
    
    eastl::vector <Render_Entity> render_entities;

    VkDescriptorPool per_scene_uniforms_descriptor_pool;
    VkDescriptorSetLayout per_scene_uniforms_descriptor_set_layout;
    VkDescriptorSet per_scene_uniforms_descriptor_sets[Render_Backend::MAX_FRAMES_IN_FLIGHT];
    Gpu_Buffer per_scene_uniform_buffers[Render_Backend::MAX_FRAMES_IN_FLIGHT];
    
    VkDescriptorPool per_object_uniforms_descriptor_pool;
    VkDescriptorSetLayout per_object_uniforms_descriptor_set_layout;
    VkDescriptorSet *per_object_uniforms_descriptor_sets;
    Gpu_Buffer *per_object_uniform_buffers;
    
    VkDescriptorPool material_uniforms_descriptor_pool;
    VkDescriptorSetLayout material_uniforms_descriptor_set_layout;

    VkPipelineLayout mesh_pipeline_layout;
    VkPipeline mesh_pipeline;
};
