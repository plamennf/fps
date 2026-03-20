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
        if (!backend->create_buffer(&per_scene_uniform_buffers[i], VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT , sizeof(Per_Scene_Uniforms), NULL)) {
            return false;
        }

        backend->update_descriptor_set(per_scene_uniforms_descriptor_sets[i], 0, &per_scene_uniform_buffers[i]);
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

    VkPushConstantRange per_object_push_constant_range = {};
    per_object_push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    per_object_push_constant_range.offset = 0;
    per_object_push_constant_range.size = sizeof(Per_Object_Uniforms);
    
    VkDescriptorSetLayout mesh_descriptor_set_layouts[] = {
        per_scene_uniforms_descriptor_set_layout,
        material_uniforms_descriptor_set_layout,
    };
    if (!backend->create_graphics_pipeline_layout(ArrayCount(mesh_descriptor_set_layouts), mesh_descriptor_set_layouts, 1, &per_object_push_constant_range, &mesh_pipeline_layout)) {
        return false;
    }
    
    Graphics_Pipeline_Info mesh_pipepline_info = {};
    
    mesh_pipepline_info.pipeline_layout = mesh_pipeline_layout;
    mesh_pipepline_info.shader_filename = "basic";
    mesh_pipepline_info.vertex_type     = RENDER_VERTEX_MESH;
    mesh_pipepline_info.blend_mode      = BLEND_MODE_OFF;
    mesh_pipepline_info.cull_mode       = CULL_MODE_BACK;
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
    MyZoneScopedN("Scene_Renderer::render");
    
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
    glm::mat4 view_matrix = get_view_matrix(&camera);

    Per_Scene_Uniforms per_scene_uniforms;
    per_scene_uniforms.projection_matrix = projection_matrix;
    per_scene_uniforms.view_matrix       = view_matrix;

    memcpy(per_scene_uniforms.lights, lights, MAX_LIGHTS * sizeof(Light));

    per_scene_uniforms.camera_position   = camera.position;
    
    backend->update_buffer(&per_scene_uniform_buffers[backend->current_frame], 0, sizeof(per_scene_uniforms), &per_scene_uniforms);
    //backend->update_descriptor_set(per_scene_uniforms_descriptor_sets[backend->current_frame], 0, &per_scene_uniform_buffers[backend->current_frame]);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline);

    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout, 0, 1, &per_scene_uniforms_descriptor_sets[backend->current_frame], 0, NULL);
    
    for (int i = 0; i < render_entities.size(); i++) {
        MyZoneScopedN("Draw single mesh");
        
        auto const &entity = render_entities[i];

        Per_Object_Uniforms per_object_uniforms = {};

        per_object_uniforms.world_matrix = entity.world_matrix;
        per_object_uniforms.scale_color  = entity.scale_color;
        
        vkCmdPushConstants(cb, mesh_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Per_Object_Uniforms), &per_object_uniforms);
        
        Mesh *mesh = entity.mesh;
        for (int i = 0; i < mesh->num_submeshes; i++) {
            Submesh *submesh = &mesh->submeshes[i];
            if (!submesh->has_gpu_data) {
                generate_gpu_data_for_submesh(submesh);
                if (!submesh->has_gpu_data) continue;
            }

            MyZoneScopedN("Draw one submesh");
            
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_layout, 1, 1, &submesh->material.descriptor_set, 0, NULL);

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cb, 0, 1, &submesh->vertex_buffer.buffer, &offset);
            vkCmdBindIndexBuffer(cb, submesh->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(cb, submesh->num_indices, 1, 0, 0, 0);
        }
    }
    
    // TODO: Move this out of here eventually. It's here because it needs to be between vkCmdBeginRendering and vkCmdEndRendering
    {
        MyZoneScopedN("ImGui Rendering");
        static float current_dt = 1.0f;
        static int frame_counter = 0;

        frame_counter++;
        if (frame_counter > 30) {
            current_dt = (float)globals.time_info.delta_time_seconds;
            frame_counter = 0;
        }
        
        globals.render_backend->imgui_begin_frame();
        ImGui::Begin("Stats");
        ImGui::Text("FPS: %d", (int)(1.0f / current_dt));
        ImGui::Text("Frame time: %.2fms", current_dt * 1000.0f);
        ImGui::End();
        globals.render_backend->imgui_end_frame(cb);
    }

    vkCmdEndRendering(cb);

    backend->image_layout_transition(cb, backend->get_current_swap_chain_image(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, color_range);

    render_entities.clear();
    num_lights = 0;
}

void Scene_Renderer::set_camera(Camera _camera) {
    camera = _camera;
}

void Scene_Renderer::add_light(Light light) {
    if (num_lights >= MAX_LIGHTS) {
        logprintf("You are trying to add too many lights! The max is: %d\n", MAX_LIGHTS);
        return;
    }

    lights[num_lights++] = light;
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
    MyZoneScopedN("Generate gpu data for submesh");
    
    if (!backend->create_buffer(&submesh->vertex_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, submesh->num_vertices * sizeof(Mesh_Vertex), submesh->vertices)) return;
    if (!backend->create_buffer(&submesh->index_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, submesh->num_indices * sizeof(u32), submesh->indices)) return;
    
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

    Material_Uniforms material_uniforms;

    material_uniforms.albedo_factor   = submesh->material.albedo_factor;
    material_uniforms.has_normal_map  = submesh->material.normal_texture && submesh->material.normal_texture != globals.white_texture;
    material_uniforms.emissive_factor = submesh->material.emissive_factor;
    material_uniforms.uses_specular_glossiness = submesh->material.uses_specular_glossiness ? 1 : 0;
    
    if (!backend->create_buffer(&submesh->material.uniform_buffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Material_Uniforms), &material_uniforms)) return;
    
    if (!backend->create_descriptor_sets(material_uniforms_descriptor_pool, material_uniforms_descriptor_set_layout, 1, &submesh->material.descriptor_set)) return;
    
    backend->update_descriptor_set(submesh->material.descriptor_set, 0, &submesh->material.uniform_buffer);
    backend->update_descriptor_set(submesh->material.descriptor_set, 1, submesh->material.albedo_texture);
    backend->update_descriptor_set(submesh->material.descriptor_set, 2, submesh->material.normal_texture);
    backend->update_descriptor_set(submesh->material.descriptor_set, 3, submesh->material.metallic_roughness_texture);
    backend->update_descriptor_set(submesh->material.descriptor_set, 4, submesh->material.ao_texture);
    backend->update_descriptor_set(submesh->material.descriptor_set, 5, submesh->material.emissive_texture);

    submesh->has_gpu_data = true;
}
