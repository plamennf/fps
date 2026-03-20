#include "pch.h"
#include "scene_renderer.h"
#include "mesh.h"
#include "../main.h"
#include "texture_registry.h"
#include "mesh_registry.h"

bool Scene_Renderer::init(Render_Backend *_backend) {
    backend = _backend;

    if (!backend->create_depth_buffer(&depth_buffer, backend->get_swap_chain_extent().width, backend->get_swap_chain_extent().height, VK_FORMAT_D32_SFLOAT)) {
        return false;
    }
    
    //
    // Create per scene vulkan objects
    //
    VkDescriptorPoolSize per_scene_uniforms_pool_sizes[1] = {};

    per_scene_uniforms_pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    per_scene_uniforms_pool_sizes[0].descriptorCount = Render_Backend::MAX_FRAMES_IN_FLIGHT;
    
    if (!backend->create_descriptor_pool(&per_scene_uniforms_descriptor_pool, ArrayCount(per_scene_uniforms_pool_sizes), per_scene_uniforms_pool_sizes, Render_Backend::MAX_FRAMES_IN_FLIGHT)) {
        return false;
    }

    VkDescriptorSetLayoutBinding per_scene_uniforms_descriptor_set_layout_bindings[1] = {};

    per_scene_uniforms_descriptor_set_layout_bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    per_scene_uniforms_descriptor_set_layout_bindings[0].descriptorCount = 1;
    per_scene_uniforms_descriptor_set_layout_bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    
    if (!backend->create_descriptor_set_layout(&per_scene_uniforms_descriptor_set_layout, ArrayCount(per_scene_uniforms_descriptor_set_layout_bindings), per_scene_uniforms_descriptor_set_layout_bindings)) {
        return false;
    }
    
    if (!backend->create_descriptor_sets(per_scene_uniforms_descriptor_pool, per_scene_uniforms_descriptor_set_layout, Render_Backend::MAX_FRAMES_IN_FLIGHT, per_scene_uniforms_descriptor_sets)) {
        return false;
    }

    for (int i = 0; i < Render_Backend::MAX_FRAMES_IN_FLIGHT; i++) {
        if (!backend->create_uniform_buffer(&per_scene_uniform_buffers[i], sizeof(Per_Scene_Uniforms), NULL)) {
            return false;
        }

        backend->update_descriptor_set(per_scene_uniforms_descriptor_sets[i], 0, &per_scene_uniform_buffers[i]);
    }

    //
    // Create per object uniforms vulkan objects
    //
    VkDescriptorPoolSize per_object_uniforms_pool_sizes[1] = {};

    per_object_uniforms_pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    per_object_uniforms_pool_sizes[0].descriptorCount = MAX_RENDER_ENTITIES * Render_Backend::MAX_FRAMES_IN_FLIGHT;

    if (!backend->create_descriptor_pool(&per_object_uniforms_descriptor_pool, ArrayCount(per_object_uniforms_pool_sizes), per_object_uniforms_pool_sizes, MAX_RENDER_ENTITIES * Render_Backend::MAX_FRAMES_IN_FLIGHT)) {
        return false;
    }
    
    VkDescriptorSetLayoutBinding per_object_uniforms_descriptor_set_layout_bindings[1] = {};

    per_object_uniforms_descriptor_set_layout_bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    per_object_uniforms_descriptor_set_layout_bindings[0].descriptorCount = 1;
    per_object_uniforms_descriptor_set_layout_bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    if (!backend->create_descriptor_set_layout(&per_object_uniforms_descriptor_set_layout, ArrayCount(per_object_uniforms_descriptor_set_layout_bindings), per_object_uniforms_descriptor_set_layout_bindings)) {
        return false;
    }

    per_object_uniforms_descriptor_sets = new VkDescriptorSet[MAX_RENDER_ENTITIES * Render_Backend::MAX_FRAMES_IN_FLIGHT];
    if (!backend->create_descriptor_sets(per_object_uniforms_descriptor_pool, per_object_uniforms_descriptor_set_layout, MAX_RENDER_ENTITIES * Render_Backend::MAX_FRAMES_IN_FLIGHT, per_object_uniforms_descriptor_sets)) {
        return false;
    }

    per_object_uniform_buffers = new Gpu_Buffer[MAX_RENDER_ENTITIES * Render_Backend::MAX_FRAMES_IN_FLIGHT];
    for (int i = 0; i < MAX_RENDER_ENTITIES * Render_Backend::MAX_FRAMES_IN_FLIGHT; i++) {
        if (!backend->create_uniform_buffer(&per_object_uniform_buffers[i], sizeof(Per_Object_Uniforms), NULL)) {
            return false;
        }

        backend->update_descriptor_set(per_object_uniforms_descriptor_sets[i], 0, &per_object_uniform_buffers[i]);
    }

    //
    // Create material uniforms vulkan objects
    //
    int max_submeshes_per_render_entity = 8;
    
    VkDescriptorPoolSize material_uniforms_pool_sizes[2] = {};

    material_uniforms_pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    material_uniforms_pool_sizes[0].descriptorCount = MAX_RENDER_ENTITIES * max_submeshes_per_render_entity;

    material_uniforms_pool_sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    material_uniforms_pool_sizes[1].descriptorCount = MAX_RENDER_ENTITIES * max_submeshes_per_render_entity * 5; // TODO: Remove this magic number make it a texture type enum for example

    if (!backend->create_descriptor_pool(&material_uniforms_descriptor_pool, ArrayCount(material_uniforms_pool_sizes), material_uniforms_pool_sizes, MAX_RENDER_ENTITIES * max_submeshes_per_render_entity)) {
        return false;
    }

    VkDescriptorSetLayoutBinding material_uniforms_descriptor_set_layout_bindings[6] = {};

    material_uniforms_descriptor_set_layout_bindings[0].binding = 0;
    material_uniforms_descriptor_set_layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    material_uniforms_descriptor_set_layout_bindings[0].descriptorCount = 1;
    material_uniforms_descriptor_set_layout_bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    for (int i = 1; i < 6; i++) {
        material_uniforms_descriptor_set_layout_bindings[i].binding = i;
        material_uniforms_descriptor_set_layout_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        material_uniforms_descriptor_set_layout_bindings[i].descriptorCount = 1;
        material_uniforms_descriptor_set_layout_bindings[i].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    if (!backend->create_descriptor_set_layout(&material_uniforms_descriptor_set_layout, ArrayCount(material_uniforms_descriptor_set_layout_bindings), material_uniforms_descriptor_set_layout_bindings)) {
        return false;
    }

    VkDescriptorSetLayout mesh_descriptor_set_layouts[] = {
        per_scene_uniforms_descriptor_set_layout,
        per_object_uniforms_descriptor_set_layout,
        material_uniforms_descriptor_set_layout,
    };
    if (!backend->create_graphics_pipeline_layout(ArrayCount(mesh_descriptor_set_layouts), mesh_descriptor_set_layouts, &mesh_pipeline_layout)) {
        return false;
    }

    Graphics_Pipeline_Info mesh_pipepline_info = {};

    mesh_pipepline_info.pipeline_layout = mesh_pipeline_layout;
    mesh_pipepline_info.shader_filename = "basic";
    mesh_pipepline_info.vertex_type     = RENDER_VERTEX_MESH;
    mesh_pipepline_info.blend_mode      = BLEND_MODE_OFF;
    mesh_pipepline_info.cull_mode       = CULL_MODE_NONE;
    mesh_pipepline_info.depth_test_mode = DEPTH_TEST_MODE_LEQUAL;
    mesh_pipepline_info.depth_write     = true;
    
    mesh_pipepline_info.color_attachment_format = backend->get_swap_chain_surface_format();
    mesh_pipepline_info.depth_attachment_format = VK_FORMAT_D32_SFLOAT;
    
    if (!backend->create_graphics_pipeline(mesh_pipepline_info, &mesh_pipeline)) {
        return false;
    }
    
    return true;
}

void Scene_Renderer::destroy_framebuffers() {
    if (depth_buffer.image) {
        backend->destroy_texture(&depth_buffer);
    }
}

bool Scene_Renderer::init_framebuffers() {
    if (!backend->create_depth_buffer(&depth_buffer, backend->get_swap_chain_extent().width, backend->get_swap_chain_extent().height, VK_FORMAT_D32_SFLOAT)) {
        return false;
    }

    return true;
}

void Scene_Renderer::render() {
    VkCommandBuffer cb = backend->get_current_command_buffer(false);
    
    VkImageSubresourceRange color_range = {};
    color_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    color_range.levelCount = VK_REMAINING_MIP_LEVELS;
    color_range.layerCount = VK_REMAINING_ARRAY_LAYERS;
    backend->image_layout_transition(cb, backend->get_current_swap_chain_image(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, color_range);

    backend->image_layout_transition(cb, depth_buffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });
    
    VkClearValue clear_color = {{{0.2f, 0.5f, 0.8f, 1.0f}}};
    VkClearValue clear_depth = {};
    clear_depth.depthStencil = { 1.0f, 0 };
    
    VkRenderingAttachmentInfoKHR color_attachment_info = {};
    color_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    color_attachment_info.imageView = backend->get_current_swap_chain_image_view();
    color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;
    color_attachment_info.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment_info.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_info.clearValue  = clear_color;

    VkRenderingAttachmentInfo depth_attachment_info = {};
    depth_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_attachment_info.imageView = depth_buffer.view;
    depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depth_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment_info.clearValue = clear_depth;
    
    VkExtent2D extent = backend->get_swap_chain_extent();
    
    VkRenderingInfoKHR rendering_info = {};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    rendering_info.renderArea.offset = {0, 0};
    rendering_info.renderArea.extent = extent;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment_info;
    rendering_info.pDepthAttachment = &depth_attachment_info;
    rendering_info.layerCount = 1;
    vkCmdBeginRendering(cb, &rendering_info);

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cb, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cb, 0, 1, &scissor);

    float aspect_ratio = extent.width / (extent.height > 0 ? (float)extent.height : 1.0f);
    glm::mat4 projection_matrix = glm::perspective(glm::radians(90.0f), aspect_ratio, 0.1f, 1000.0f);
    projection_matrix[1][1] *= -1;
    glm::mat4 view_matrix = glm::mat4(1.0f);

    Per_Scene_Uniforms per_scene_uniforms;
    per_scene_uniforms.projection_matrix = projection_matrix;
    per_scene_uniforms.view_matrix       = view_matrix;

    backend->update_buffer(&per_scene_uniform_buffers[backend->current_frame], sizeof(per_scene_uniforms), &per_scene_uniforms);
    //backend->update_descriptor_set(per_scene_uniforms_descriptor_sets[backend->current_frame], 0, &per_scene_uniform_buffers[backend->current_frame]);
    
    for (int i = 0; i < render_entities.size(); i++) {
        auto const &entity = render_entities[i];

        Per_Object_Uniforms per_object_uniforms = {};

        per_object_uniforms.world_matrix = entity.world_matrix;
        per_object_uniforms.scale_color  = entity.scale_color;
        
        int current_index = i * Render_Backend::MAX_FRAMES_IN_FLIGHT + backend->current_frame;
        backend->update_buffer(&per_object_uniform_buffers[current_index], sizeof(Per_Object_Uniforms), &per_object_uniforms);
        //update_descriptor_set(&per_object_uniforms_descriptor_sets, 0, &per_object_uniform_buffers[current_index]);
        
        Mesh *mesh = entity.mesh;
        for (int i = 0; i < mesh->num_submeshes; i++) {
            Submesh *submesh = &mesh->submeshes[i];
            if (!submesh->has_gpu_data) {
                generate_gpu_data_for_submesh(submesh);
                if (!submesh->has_gpu_data) continue;
            }

            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline);

            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout, 0, 1, &per_scene_uniforms_descriptor_sets[backend->current_frame], 0, NULL);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout, 1, 1, &per_object_uniforms_descriptor_sets[current_index], 0, NULL);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout, 2, 1, &submesh->material.descriptor_set, 0, NULL);

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cb, 0, 1, &submesh->vertex_buffer.buffer, &offset);
            vkCmdBindIndexBuffer(cb, submesh->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(cb, submesh->num_indices, 1, 0, 0, 0);
        }
    }
    
    vkCmdEndRendering(cb);

    backend->image_layout_transition(cb, backend->get_current_swap_chain_image(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, color_range);

    render_entities.clear();
}

void Scene_Renderer::add_render_entity(Mesh *mesh, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 scale_color) {
    glm::mat4 world = glm::mat4(1.0f);
    world = glm::translate(world, position);
    world = glm::rotate(world, rotation.x, glm::vec3(1, 0, 0));
    world = glm::rotate(world, rotation.y, glm::vec3(0, 1, 0));
    world = glm::rotate(world, rotation.z, glm::vec3(0, 0, 1));
    world = glm::scale(world, scale);

    Render_Entity entity;

    entity.world_matrix = world;
    entity.mesh         = mesh;
    entity.scale_color  = scale_color;
    
    render_entities.push_back(entity);
}

void Scene_Renderer::generate_gpu_data_for_submesh(Submesh *submesh) {
    if (!backend->create_vertex_buffer(&submesh->vertex_buffer, submesh->num_vertices * sizeof(Mesh_Vertex), submesh->vertices)) return;
    if (!backend->create_index_buffer(&submesh->index_buffer, submesh->num_indices * sizeof(u32), submesh->indices)) return;
    if (!backend->create_uniform_buffer(&submesh->material.uniform_buffer, sizeof(Material), &submesh->material)) return;
    if (!backend->create_descriptor_sets(material_uniforms_descriptor_pool, material_uniforms_descriptor_set_layout, 1, &submesh->material.descriptor_set)) return;
    
    submesh->material.albedo_texture = globals.white_texture;
    if (submesh->material.albedo_texture_name) {
        submesh->material.albedo_texture = globals.texture_registry->find_or_load(submesh->material.albedo_texture_name);
    }

    submesh->material.normal_texture = globals.white_texture;
    if (submesh->material.normal_texture_name) {
        submesh->material.normal_texture = globals.texture_registry->find_or_load(submesh->material.normal_texture_name);
    }

    submesh->material.metallic_roughness_texture = globals.white_texture;
    if (submesh->material.metallic_roughness_texture_name) {
        submesh->material.metallic_roughness_texture = globals.texture_registry->find_or_load(submesh->material.metallic_roughness_texture_name);
    }

    submesh->material.ao_texture = globals.white_texture;
    if (submesh->material.ao_texture_name) {
        submesh->material.ao_texture = globals.texture_registry->find_or_load(submesh->material.ao_texture_name);
    }

    submesh->material.emissive_texture = globals.white_texture;
    if (submesh->material.emissive_texture_name) {
        submesh->material.emissive_texture = globals.texture_registry->find_or_load(submesh->material.emissive_texture_name);
    }
    
    backend->update_descriptor_set(submesh->material.descriptor_set, 0, &submesh->material.uniform_buffer);
    backend->update_descriptor_set(submesh->material.descriptor_set, 1, submesh->material.albedo_texture);
    backend->update_descriptor_set(submesh->material.descriptor_set, 2, submesh->material.normal_texture);
    backend->update_descriptor_set(submesh->material.descriptor_set, 3, submesh->material.metallic_roughness_texture);
    backend->update_descriptor_set(submesh->material.descriptor_set, 4, submesh->material.ao_texture);
    backend->update_descriptor_set(submesh->material.descriptor_set, 5, submesh->material.emissive_texture);

    submesh->has_gpu_data = true;
}
