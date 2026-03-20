#pragma once

struct SDL_Window;

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

struct Mesh_Vertex {
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

struct Immediate_Vertex {
    glm::vec2 position;
    glm::vec4 color;
    glm::vec2 uv;
};

struct Queue_Family_Indices {
    u32 graphics_family;
    bool has_graphics_family;
    u32 present_family;
    bool has_present_family;
};

struct Swap_Chain_Support_Details {
    VkSurfaceCapabilitiesKHR capabilities;

    int num_formats;
    VkSurfaceFormatKHR *formats;

    int num_present_modes;
    VkPresentModeKHR *present_modes;
};

struct Gpu_Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = {};
    VmaAllocationInfo allocation_info = {};
    VkDeviceSize size = 0;
};

struct Texture {
    int width;
    int height;
    
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = {};
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
};

enum Render_Vertex_Type {
    RENDER_VERTEX_IMMEDIATE,
    RENDER_VERTEX_MESH,
};

enum Blend_Mode {
    BLEND_MODE_OFF,
    BLEND_MODE_ALPHA,
};

enum Cull_Mode {
    CULL_MODE_NONE,
    CULL_MODE_BACK,
    CULL_MODE_FRONT,
};

enum Depth_Test_Mode {
    DEPTH_TEST_MODE_OFF,
    DEPTH_TEST_MODE_LEQUAL,
};

struct Graphics_Pipeline_Info {
    VkPipelineLayout pipeline_layout;
    char *shader_filename;
    Render_Vertex_Type vertex_type;
    Blend_Mode blend_mode;
    Cull_Mode cull_mode;
    Depth_Test_Mode depth_test_mode;
    bool depth_write;

    VkFormat color_attachment_format;
    VkFormat depth_attachment_format;
};

struct Render_Backend {
    static const int MAX_FRAMES_IN_FLIGHT = 2;

    bool init(SDL_Window *window);

    void device_wait_idle();
    void wait_on_all_fences();
    
    bool begin_frame();
    bool end_frame();
    
    bool load_texture(Texture *texture, char *filepath);
    bool create_texture(Texture *texture, int width, int height, VkFormat format, u8 *data, char *filepath = "(unknown)");
    
    VkCommandBuffer get_current_command_buffer(bool reset = true);
    VkImage get_current_swap_chain_image();
    VkImageView get_current_swap_chain_image_view();
    inline VkExtent2D get_swap_chain_extent() { return swap_chain_extent; }
    inline VkFormat get_swap_chain_surface_format() { return swap_chain_surface_format.format; };
    
    bool create_vertex_buffer(Gpu_Buffer *buffer, VkDeviceSize size, void *initial_data);
    bool create_index_buffer(Gpu_Buffer *buffer, VkDeviceSize size, void *initial_data);
    bool create_uniform_buffer(Gpu_Buffer *buffer, VkDeviceSize size, void *initial_data);
    bool update_buffer(Gpu_Buffer *buffer, VkDeviceSize offset, VkDeviceSize size, void *data);
    void destroy_buffer(Gpu_Buffer *buffer);

    bool create_descriptor_set_layout(VkDescriptorSetLayout *layout, int num_descriptor_layout_bindings, VkDescriptorSetLayoutBinding *descriptor_layout_bindings);
    bool create_descriptor_pool(VkDescriptorPool *pool, int num_pool_sizes, VkDescriptorPoolSize *pool_sizes, int max_descriptor_sets);
    bool create_descriptor_sets(VkDescriptorPool descriptor_pool, VkDescriptorSetLayout layout, int num_descriptor_sets, VkDescriptorSet *descriptor_sets);
    void update_descriptor_set(VkDescriptorSet set, int binding_slot, Gpu_Buffer *buffer);
    void update_descriptor_set(VkDescriptorSet set, int binding_slot, Texture *texture);
    
    VkShaderModule create_shader_module(s64 code_size, const char *code);
    bool create_graphics_pipeline_layout(int num_descriptor_set_layouts, VkDescriptorSetLayout *descriptor_set_layouts, int num_push_constant_ranges, VkPushConstantRange *push_constant_ranges, VkPipelineLayout *result);
    bool create_graphics_pipeline(Graphics_Pipeline_Info info, VkPipeline *result);
    bool recreate_swap_chain();
    
    void image_layout_transition(VkCommandBuffer buffer, VkImage image, VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkImageLayout old_layout, VkImageLayout new_layout, VkImageSubresourceRange subresource_range);
    void image_layout_transition(VkCommandBuffer buffer, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkImageSubresourceRange subresource_range);

    bool create_depth_buffer(Texture *texture, int width, int height, VkFormat format);
    void destroy_texture(Texture *texture);

    void imgui_init();
    void imgui_begin_frame();
    void imgui_end_frame(VkCommandBuffer cb);
    
private:
    bool create_instance();
    bool create_debug_messenger();
    bool create_surface();
    bool select_physical_device();
    bool create_logical_device();
    bool init_vma();
    bool create_swap_chain();
    bool create_image_views();
    bool create_command_pool();
    bool create_command_buffer();
    bool create_sync_objects();

    void destroy_swap_chain();
    
    bool check_extension_support(const char **user_extensions, int num_user_extensions);
    bool check_validation_layer_support();
    bool check_device_extension_support(VkPhysicalDevice device, int num_device_extensions, const char **device_extensions);
    
    Queue_Family_Indices find_queue_families(VkPhysicalDevice device);
    Swap_Chain_Support_Details query_swap_chain_support(VkPhysicalDevice device);
    VkSurfaceFormatKHR choose_swap_surface_format(int num_available_formats, VkSurfaceFormatKHR *available_formats);
    VkPresentModeKHR choose_swap_present_mode(int num_available_present_modes, VkPresentModeKHR *available_present_modes);
    VkExtent2D choose_swap_extent(SDL_Window *window, VkSurfaceCapabilitiesKHR capabilities);
    bool is_device_suitable(VkPhysicalDevice device);
    int rate_device_suitability(VkPhysicalDevice device);
    
private:
    SDL_Window *window;

    int num_extensions;
    const char **extensions;

    int num_device_extensions;
    const char **device_extensions;
    
    bool validation_layers_enabled;
    int num_validation_layers;
    const char **validation_layers;
    
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    Queue_Family_Indices queue_family_indices;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;

    VmaAllocator allocator;

    Swap_Chain_Support_Details swap_chain_support_details;
    VkSurfaceFormatKHR swap_chain_surface_format;
    VkPresentModeKHR swap_chain_present_mode;
    VkExtent2D swap_chain_extent;
    VkSwapchainKHR swap_chain;
    u32 num_swap_chain_images;
    VkImage *swap_chain_images;
    VkImageView *swap_chain_image_views; // Same count as swap_chain_images
    
    VkCommandPool command_pool;
    VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandBuffer copy_command_buffer;
    
    VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore *render_finished_semaphores;//[MAX_FRAMES_IN_FLIGHT]; // Same count as swap_chain_images
    VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];

public:
    u32 current_frame = 0;
    u32 image_index = 0;
};
