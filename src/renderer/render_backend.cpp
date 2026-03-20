#include "pch.h"
#define VMA_IMPLEMENTATION
#include "render_backend.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data) {
    logprintf("Validation layers: %s\n", callback_data->pMessage);
    return VK_FALSE;
}

bool Render_Backend::init(SDL_Window *_window) {
    window = _window;

#if defined(BUILD_DEBUG) || defined(BUILD_RELEASE)
    validation_layers_enabled = true;
#else
    validation_layers_enabled = false;
#endif

    if (validation_layers_enabled) {
        num_validation_layers = 1;
        validation_layers     = new const char *[num_validation_layers];
        validation_layers[0]  = "VK_LAYER_KHRONOS_validation";
    } else {
        num_validation_layers = 0;
        validation_layers     = NULL;
    }

    u32 num_sdl_extensions = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &num_sdl_extensions, NULL);

    const char **sdl_extensions = new const char *[num_sdl_extensions];
    defer { delete [] sdl_extensions; };
    SDL_Vulkan_GetInstanceExtensions(window, &num_sdl_extensions, sdl_extensions);

    num_extensions = (int)num_sdl_extensions + 1;
    if (validation_layers) {
        num_extensions += 1;
    }

    extensions = new const char *[num_extensions];
    memcpy(extensions, sdl_extensions, num_sdl_extensions * sizeof(const char *));
    extensions[num_extensions - 2] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
    if (validation_layers) {
        extensions[num_extensions - 1] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }

    num_device_extensions = 2;
    device_extensions = new const char *[num_device_extensions];
    device_extensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    device_extensions[1] = VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
    
    if (!create_instance()) return false;

    if (validation_layers_enabled) {
        if (!create_debug_messenger()) return false;
    }

    if (!create_surface()) return false;
    if (!select_physical_device()) return false;
    if (!create_logical_device()) return false;

    logprintf("Graphics Queue Family Index: %d\n", queue_family_indices.graphics_family);
    logprintf("Present Queue Family Index: %d\n", queue_family_indices.present_family);
    
    if (!init_vma()) return false;
    if (!create_swap_chain()) return false;
    if (!create_image_views()) return false;
    if (!create_command_pool()) return false;
    if (!create_command_buffer()) return false;
    if (!create_sync_objects()) return false;
    
    return true;
}

void Render_Backend::device_wait_idle() {
    vkDeviceWaitIdle(device);
}

void Render_Backend::wait_on_all_fences() {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkWaitForFences(device, 1, &in_flight_fences[i], VK_TRUE, UINT64_MAX);
    }
}

static VkDebugUtilsMessengerCreateInfoEXT get_debug_messenger_create_info() {
    VkDebugUtilsMessengerCreateInfoEXT create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = vk_debug_callback;
    return create_info;
}

bool Render_Backend::create_instance() {
    if (validation_layers_enabled && !check_validation_layer_support()) {
        return false;
    }
    
    VkApplicationInfo app_info  = {};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = "First-Person Shooter!";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName        = "No Engine";
    app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion         = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info = {};
    create_info.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    
    if (!check_extension_support(extensions, num_extensions)) {
        return false;
    }

    create_info.enabledExtensionCount   = num_extensions;
    create_info.ppEnabledExtensionNames = extensions;

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = get_debug_messenger_create_info();
    if (validation_layers_enabled) {
        create_info.enabledLayerCount   = (u32)num_validation_layers;
        create_info.ppEnabledLayerNames = validation_layers;
        create_info.pNext               = &debug_create_info;
    } else {
        create_info.enabledLayerCount   = 0;
    }

    if (vkCreateInstance(&create_info, 0, &instance) != VK_SUCCESS) {
        logprintf("Failed to create vulkan instance!\n");
        return false;
    }

    return true;
}

bool Render_Backend::check_extension_support(const char **user_extensions, int num_user_extensions) {
    u32 num_extensions = 0;
    vkEnumerateInstanceExtensionProperties(0, &num_extensions, 0);

    VkExtensionProperties *extensions = new VkExtensionProperties[num_extensions];
    defer { delete[] extensions; };
    vkEnumerateInstanceExtensionProperties(0, &num_extensions, extensions);

    for (int i = 0; i < num_user_extensions; i++) {
        bool is_available = false;
        for (u32 j = 0; j < num_extensions; j++) {
            VkExtensionProperties extension = extensions[j];
            if (strings_match(extension.extensionName, user_extensions[i])) {
                is_available = true;
                break;
            }
        }

        if (!is_available) {
            logprintf("Extension '%s' is not available\n", user_extensions[i]);
            return false;
        }
    }

    return true;
}

bool Render_Backend::check_validation_layer_support() {
    u32 num_layers = 0;
    vkEnumerateInstanceLayerProperties(&num_layers, 0);

    VkLayerProperties *available_layers = new VkLayerProperties[num_layers];
    defer { delete[] available_layers; };
    vkEnumerateInstanceLayerProperties(&num_layers, available_layers);

    for (int i = 0; i < num_validation_layers; i++) {
        const char *layer_name = validation_layers[i];
        bool layer_found = false;

        for (u32 j = 0; j < num_layers; j++) {
            VkLayerProperties layer_properties = available_layers[j];
            if (strings_match(layer_name, layer_properties.layerName)) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            logprintf("Validation Layer '%s' is not available\n", layer_name);
            return false;
        }
    }
    
    return true;
}

bool Render_Backend::create_debug_messenger() {
    VkDebugUtilsMessengerCreateInfoEXT create_info = get_debug_messenger_create_info();
    
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) {
        if (func(instance, &create_info, 0, &debug_messenger) != VK_SUCCESS) {
            logprintf("Failed to create the debug messenger\n");
            return false;
        }
    } else {
        logprintf("Failed to get vkCreateDebugUtilsMessengerEXT\n");
        return false;
    }

    return true;
}

bool Render_Backend::create_surface() {
    if (SDL_Vulkan_CreateSurface(window, instance, &surface) != SDL_TRUE) {
        logprintf("Failed to create vulkan surface!\n");
        return false;
    }
    return true;
}

Queue_Family_Indices Render_Backend::find_queue_families(VkPhysicalDevice device) {
    Queue_Family_Indices indices = {};

    u32 num_queue_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, 0);

    VkQueueFamilyProperties *queue_families = new VkQueueFamilyProperties[num_queue_families];
    defer { delete[] queue_families; };
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, queue_families);

    for (u32 i = 0; i < num_queue_families; i++) {
        VkQueueFamilyProperties queue_family = queue_families[i];
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.has_graphics_family = true;
            continue; // Just in case present_family is the same as graphics_family.
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);

        if (present_support) {
            indices.present_family = i;
            indices.has_present_family = true;
        }

        if (indices.has_graphics_family && indices.has_present_family) {
            break;
        }
    }
    
    return indices;
}

bool Render_Backend::check_device_extension_support(VkPhysicalDevice device, int num_device_extensions, const char **device_extensions) {
    u32 num_extensions = 0;
    vkEnumerateDeviceExtensionProperties(device, 0, &num_extensions, 0);

    VkExtensionProperties *available_extensions = new VkExtensionProperties[num_extensions];
    defer { delete[] available_extensions; };
    vkEnumerateDeviceExtensionProperties(device, 0, &num_extensions, available_extensions);

    for (int i = 0; i < num_device_extensions; i++) {
        bool is_available = false;
        for (u32 j = 0; j < num_extensions; j++) {
            VkExtensionProperties extension = available_extensions[j];
            if (strings_match(extension.extensionName, device_extensions[i])) {
                is_available = true;
                break;
            }
        }

        if (!is_available) {
            logprintf("Device extension '%s' is not available\n", device_extensions[i]);
            return false;
        }
    }

    return true;
}

Swap_Chain_Support_Details Render_Backend::query_swap_chain_support(VkPhysicalDevice device) {
    Swap_Chain_Support_Details details = {};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    u32 num_formats = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &num_formats, 0);
    if (num_formats > 0) {
        details.num_formats = num_formats;
        details.formats = new VkSurfaceFormatKHR[details.num_formats];
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &num_formats, details.formats);
    } else {
        details.num_formats = 0;
        details.formats = 0;
    }

    u32 num_present_modes = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &num_present_modes, 0);
    if (num_present_modes > 0) {
        details.num_present_modes = num_present_modes;
        details.present_modes = new VkPresentModeKHR[details.num_present_modes];
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &num_present_modes, details.present_modes);
    } else {
        details.num_present_modes = 0;
        details.present_modes = 0;
    }
    
    return details;
}

VkSurfaceFormatKHR Render_Backend::choose_swap_surface_format(int num_available_formats, VkSurfaceFormatKHR *available_formats) {
    for (int i = 0; i < num_available_formats; i++) {
        VkSurfaceFormatKHR available_format = available_formats[i];
        if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_format;
        }
    }

    return available_formats[0];
}

VkPresentModeKHR Render_Backend::choose_swap_present_mode(int num_available_present_modes, VkPresentModeKHR *available_present_modes) {
    for (int i = 0; i < num_available_present_modes; i++) {
        VkPresentModeKHR available_present_mode = available_present_modes[i];
        if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available_present_mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Render_Backend::choose_swap_extent(SDL_Window *window, VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    VkExtent2D actual_extent = {
        (u32)width,
        (u32)height,
    };

    clamp(&actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    clamp(&actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actual_extent;
}

bool Render_Backend::is_device_suitable(VkPhysicalDevice device) {
    Queue_Family_Indices indices = find_queue_families(device);
    bool extensions_supported = check_device_extension_support(device, num_device_extensions, device_extensions);

    bool swap_chain_adequate = false;
    if (extensions_supported) {
        Swap_Chain_Support_Details details = query_swap_chain_support(device);
        swap_chain_adequate = details.num_formats > 0 && details.num_present_modes > 0;
    }
    
    return indices.has_graphics_family && indices.has_present_family && extensions_supported && swap_chain_adequate;
}

bool Render_Backend::select_physical_device() {
    u32 num_devices = 0;
    vkEnumeratePhysicalDevices(instance, &num_devices, 0);

    if (num_devices <= 0) {
        logprintf("Failed to find GPUs with Vulkan support!\n");
        return false;
    }

    VkPhysicalDevice *devices = new VkPhysicalDevice[num_devices];
    defer { delete[] devices; };
    vkEnumeratePhysicalDevices(instance, &num_devices, devices);

    for (u32 i = 0; i < num_devices; i++) {
        VkPhysicalDevice device = devices[i];
        if (is_device_suitable(device)) {
            physical_device = device;
            break;
        }
    }

    if (!physical_device) {
        logprintf("Failed to find a suitable GPU!\n");
        return false;
    }

    queue_family_indices = find_queue_families(physical_device);
    
    return true;
}

bool Render_Backend::create_logical_device() {
    float queue_priority = 1.0f;
    
    VkDeviceQueueCreateInfo graphics_queue_create_info = {};
    graphics_queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_create_info.queueFamilyIndex = queue_family_indices.graphics_family;
    graphics_queue_create_info.queueCount       = 1;
    graphics_queue_create_info.pQueuePriorities = &queue_priority;

    VkDeviceQueueCreateInfo present_queue_create_info = {};
    present_queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    present_queue_create_info.queueFamilyIndex = queue_family_indices.present_family;
    present_queue_create_info.queueCount       = 1;
    present_queue_create_info.pQueuePriorities = &queue_priority;

    VkDeviceQueueCreateInfo queue_create_infos[] = {
        graphics_queue_create_info,
        present_queue_create_info,
    };
    
    VkPhysicalDeviceFeatures device_features = {};

    VkDeviceCreateInfo create_info = {};
    
    create_info.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = ArrayCount(queue_create_infos);
    create_info.pQueueCreateInfos    = queue_create_infos;
    create_info.pEnabledFeatures     = &device_features;
    
    if (validation_layers_enabled) {
        create_info.enabledLayerCount   = num_validation_layers;
        create_info.ppEnabledLayerNames = validation_layers;
    }

    create_info.enabledExtensionCount   = num_device_extensions;
    create_info.ppEnabledExtensionNames = device_extensions;

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature = {};
    dynamic_rendering_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamic_rendering_feature.dynamicRendering = VK_TRUE;

    create_info.pNext = &dynamic_rendering_feature;
    
    if (vkCreateDevice(physical_device, &create_info, 0, &device) != VK_SUCCESS) {
        logprintf("Failed to create vulkan logical device\n");
        return false;
    }

    vkGetDeviceQueue(device, queue_family_indices.graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(device, queue_family_indices.present_family,  0, &present_queue);
    
    return true;
}

bool Render_Backend::init_vma() {
    VmaVulkanFunctions vk_functions = {};
    vk_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vk_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vk_functions.vkCreateImage = vkCreateImage;

    VmaAllocatorCreateInfo allocator_create_info = {};
    allocator_create_info.flags = 0;//VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocator_create_info.physicalDevice = physical_device;
    allocator_create_info.device = device;
    allocator_create_info.pVulkanFunctions = &vk_functions;
    allocator_create_info.instance = instance;

    if (vmaCreateAllocator(&allocator_create_info, &allocator) != VK_SUCCESS) {
        logprintf("Failed to create vma allocator");
        return false;
    }

    return true;
}

bool Render_Backend::create_swap_chain() {
    swap_chain_support_details = query_swap_chain_support(physical_device);

    swap_chain_surface_format = choose_swap_surface_format(swap_chain_support_details.num_formats, swap_chain_support_details.formats);
    swap_chain_present_mode = choose_swap_present_mode(swap_chain_support_details.num_present_modes, swap_chain_support_details.present_modes);
    swap_chain_extent = choose_swap_extent(window, swap_chain_support_details.capabilities);

    u32 num_images = swap_chain_support_details.capabilities.minImageCount + 1;
    if (swap_chain_support_details.capabilities.maxImageCount > 0 && num_images > swap_chain_support_details.capabilities.maxImageCount) {
        num_images = swap_chain_support_details.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface          = surface;
    create_info.minImageCount    = num_images;
    create_info.imageFormat      = swap_chain_surface_format.format;
    create_info.imageColorSpace  = swap_chain_surface_format.colorSpace;
    create_info.imageExtent      = swap_chain_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (queue_family_indices.graphics_family != queue_family_indices.present_family) {
        u32 _queue_family_indices[] = {
            queue_family_indices.graphics_family,
            queue_family_indices.present_family,
        };
        
        create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = ArrayCount(_queue_family_indices);
        create_info.pQueueFamilyIndices   = _queue_family_indices;
    } else {
        create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices   = 0;
    }

    create_info.preTransform   = swap_chain_support_details.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode    = swap_chain_present_mode;
    create_info.clipped        = VK_TRUE;
    create_info.oldSwapchain   = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &create_info, 0, &swap_chain) != VK_SUCCESS) {
        logprintf("Failed to create the swap chain!\n");
        return false;
    }

    vkGetSwapchainImagesKHR(device, swap_chain, &num_swap_chain_images, 0);
    swap_chain_images = new VkImage[num_swap_chain_images];
    vkGetSwapchainImagesKHR(device, swap_chain, &num_swap_chain_images, swap_chain_images);
    
    return true;
}

bool Render_Backend::create_image_views() {
    swap_chain_image_views = new VkImageView[num_swap_chain_images];
    for (u32 i = 0; i < num_swap_chain_images; i++) {
        VkImageViewCreateInfo create_info = {};
        create_info.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image        = swap_chain_images[i];
        create_info.viewType     = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format       = swap_chain_surface_format.format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel   = 0;
        create_info.subresourceRange.levelCount     = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device, &create_info, 0, &swap_chain_image_views[i]) != VK_SUCCESS) {
            logprintf("Failed to create image view(%d)!\n", i);
            return false;
        }
    }

    return true;
}

VkShaderModule Render_Backend::create_shader_module(s64 code_size, const char *code) {
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code_size;
    create_info.pCode    = (u32 *)code;

    VkShaderModule shader_module = 0;
    if (vkCreateShaderModule(device, &create_info, 0, &shader_module) != VK_SUCCESS) {
        logprintf("Failed to create shader module!\n");
        return 0;
    }
    return shader_module;
}

bool Render_Backend::create_graphics_pipeline_layout(int num_descriptor_set_layouts, VkDescriptorSetLayout *descriptor_set_layouts, VkPipelineLayout *result) {
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = num_descriptor_set_layouts;
    pipeline_layout_info.pSetLayouts = descriptor_set_layouts;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = 0;

    if (vkCreatePipelineLayout(device, &pipeline_layout_info, 0, result) != VK_SUCCESS) {
        logprintf("Failed to create pipeline layout!\n");
        return false;
    }

    return true;
}

bool Render_Backend::create_graphics_pipeline(Graphics_Pipeline_Info info, VkPipeline *result) {
    char vert_filepath[1024];
    snprintf(vert_filepath, sizeof(vert_filepath), "data/shaders/compiled/%s.vert.spv", info.shader_filename);
    
    s64 vert_shader_code_size = 0;
    const char *vert_shader_code = read_entire_file(vert_filepath, &vert_shader_code_size);
    if (!vert_shader_code) {
        logprintf("Failed to read file '%s'.\n", vert_filepath);
        return false;
    }
    defer { delete[] vert_shader_code; };

    char frag_filepath[1024];
    snprintf(frag_filepath, sizeof(frag_filepath), "data/shaders/compiled/%s.frag.spv", info.shader_filename);
    
    s64 frag_shader_code_size = 0;
    const char *frag_shader_code = read_entire_file("data/shaders/compiled/basic.frag.spv", &frag_shader_code_size);
    if (!frag_shader_code) {
        logprintf("Failed to read file '%s'.\n", frag_filepath);
        return false;
    }
    defer { delete[] frag_shader_code; };
    
    VkShaderModule vert_shader_module = create_shader_module(vert_shader_code_size, vert_shader_code);
    if (!vert_shader_module) {
        return false;
    }
    defer { vkDestroyShaderModule(device, vert_shader_module, 0); };
    
    VkShaderModule frag_shader_module = create_shader_module(frag_shader_code_size, frag_shader_code);
    if (!frag_shader_module) {
        return false;
    }
    defer { vkDestroyShaderModule(device, frag_shader_module, 0); };
    
    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
    vert_shader_stage_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName  = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
    frag_shader_stage_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName  = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

    VkVertexInputBindingDescription vertex_binding = {};
    vertex_binding.binding   = 0;

    eastl::vector<VkVertexInputAttributeDescription> vertex_attributes;
    switch (info.vertex_type) {
        case RENDER_VERTEX_IMMEDIATE: {
            vertex_binding.stride    = sizeof(Immediate_Vertex);

            VkVertexInputAttributeDescription attribute = {};

            attribute.location = 0;
            attribute.format   = VK_FORMAT_R32G32_SFLOAT;
            attribute.offset   = offsetof(Immediate_Vertex, position);
            vertex_attributes.push_back(attribute);

            attribute.location = 1;
            attribute.format   = VK_FORMAT_R32G32B32A32_SFLOAT;
            attribute.offset   = offsetof(Immediate_Vertex, color);
            vertex_attributes.push_back(attribute);

            attribute.location = 2;
            attribute.format   = VK_FORMAT_R32G32_SFLOAT;
            attribute.offset   = offsetof(Immediate_Vertex, uv);
            vertex_attributes.push_back(attribute);
        } break;

        case RENDER_VERTEX_MESH: {
            vertex_binding.stride    = sizeof(Mesh_Vertex);

            VkVertexInputAttributeDescription attribute = {};

            attribute.location = 0;
            attribute.format   = VK_FORMAT_R32G32B32_SFLOAT;
            attribute.offset   = offsetof(Mesh_Vertex, position);
            vertex_attributes.push_back(attribute);

            attribute.location = 1;
            attribute.format   = VK_FORMAT_R32G32B32A32_SFLOAT;
            attribute.offset   = offsetof(Mesh_Vertex, color);
            vertex_attributes.push_back(attribute);

            attribute.location = 2;
            attribute.format   = VK_FORMAT_R32G32_SFLOAT;
            attribute.offset   = offsetof(Mesh_Vertex, uv);
            vertex_attributes.push_back(attribute);

            attribute.location = 3;
            attribute.format   = VK_FORMAT_R32G32B32_SFLOAT;
            attribute.offset   = offsetof(Mesh_Vertex, normal);
            vertex_attributes.push_back(attribute);
            
            attribute.location = 4;
            attribute.format   = VK_FORMAT_R32G32B32_SFLOAT;
            attribute.offset   = offsetof(Mesh_Vertex, tangent);
            vertex_attributes.push_back(attribute);
            
            attribute.location = 5;
            attribute.format   = VK_FORMAT_R32G32B32_SFLOAT;
            attribute.offset   = offsetof(Mesh_Vertex, bitangent);
            vertex_attributes.push_back(attribute);
        } break;
    }
    
    vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &vertex_binding;
    vertex_input_info.vertexAttributeDescriptionCount = (u32)vertex_attributes.size();
    vertex_input_info.pVertexAttributeDescriptions = vertex_attributes.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;
    
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = ArrayCount(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;

    switch (info.cull_mode) {
        case CULL_MODE_NONE: {
            rasterizer.cullMode = VK_CULL_MODE_NONE;
        } break;

        case CULL_MODE_BACK: {
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        } break;

        case CULL_MODE_FRONT: {
            rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        } break;
    }
    
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = 0;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    switch (info.blend_mode) {
        case BLEND_MODE_OFF: {
            color_blend_attachment.blendEnable = VK_FALSE;
        } break;

        case BLEND_MODE_ALPHA: {
            color_blend_attachment.blendEnable = VK_TRUE;
        } break;
    }

    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    VkPipelineRenderingCreateInfoKHR pipeline_create = {};
    pipeline_create.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipeline_create.colorAttachmentCount = 1;
    //pipeline_create.pColorAttachmentFormats = &swap_chain_surface_format.format;
    pipeline_create.pColorAttachmentFormats = &info.color_attachment_format;

    if (info.depth_attachment_format != VK_FORMAT_UNDEFINED) {
        pipeline_create.depthAttachmentFormat = info.depth_attachment_format;
    }
    
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable  = info.depth_test_mode != DEPTH_TEST_MODE_OFF;
    depth_stencil.depthWriteEnable = info.depth_write;

    switch (info.depth_test_mode) {
        case DEPTH_TEST_MODE_LEQUAL: {
            depth_stencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
        }
    }
    depth_stencil.minDepthBounds   = 0.0f;
    depth_stencil.maxDepthBounds   = 1.0f;
    
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext               = &pipeline_create;
    pipeline_info.stageCount          = ArrayCount(shader_stages);
    pipeline_info.pStages             = shader_stages;
    pipeline_info.pVertexInputState   = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState      = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState   = &multisampling;
    pipeline_info.pDepthStencilState  = &depth_stencil;
    pipeline_info.pColorBlendState    = &color_blending;
    pipeline_info.pDynamicState       = &dynamic_state;
    pipeline_info.layout              = info.pipeline_layout;
    pipeline_info.renderPass          = VK_NULL_HANDLE;//render_pass;
    pipeline_info.subpass             = 0;
    pipeline_info.basePipelineHandle  = 0;
    pipeline_info.basePipelineIndex   = -1;

    if (vkCreateGraphicsPipelines(device, 0, 1, &pipeline_info, 0, result) != VK_SUCCESS) {
        logprintf("Failed to create the graphics pipeline!\n");
        return false;
    }
    
    return true;
}

bool Render_Backend::create_command_pool() {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = queue_family_indices.graphics_family;
    if (vkCreateCommandPool(device, &pool_info, 0, &command_pool) != VK_SUCCESS) {
        logprintf("Failed to create command pool!\n");
        return false;
    }

    return true;
}

bool Render_Backend::create_command_buffer() {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool        = command_pool;
    alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 2;
    if (vkAllocateCommandBuffers(device, &alloc_info, command_buffers) != VK_SUCCESS) {
        logprintf("Failed to allocate command buffers!\n");
        return false;
    }

    alloc_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &alloc_info, &copy_command_buffer) != VK_SUCCESS) {
        logprintf("Failed to allocate copy command buffers!\n");
        return false;
    }
    
    return true;
}

bool Render_Backend::create_sync_objects() {
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;    
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphore_info, 0, &image_available_semaphores[i]) != VK_SUCCESS) {
            logprintf("Failed to create image available semaphore!\n");
            return false;
        }
        
        if (vkCreateFence(device, &fence_info, 0, &in_flight_fences[i]) != VK_SUCCESS) {
            logprintf("Failed to create semaphores!\n");
            return false;
        }
    }

    render_finished_semaphores = new VkSemaphore[num_swap_chain_images];

    for (u32 i = 0; i < num_swap_chain_images; i++) {
        if (vkCreateSemaphore(device, &semaphore_info, 0, &render_finished_semaphores[i]) != VK_SUCCESS) {
            logprintf("Failed to create render finished semaphore!\n");
            return false;
        }
    }
    
    return true;
}

void Render_Backend::destroy_swap_chain() {
    for (u32 i = 0; i < num_swap_chain_images; i++) {
        VkImageView image_view = swap_chain_image_views[i];
        vkDestroyImageView(device, image_view, 0);
    }
    
    vkDestroySwapchainKHR(device, swap_chain, 0);
}

bool Render_Backend::recreate_swap_chain() {
    /*
    int width = 0, height = 0;
    SDL_GetWindowSize(window, &width, &height);
    while (width == 0 || height == 0) {
        SDL_GetWindowSize(window, &width, &height);
        SDL_WaitEvent(NULL);
    }
    */
    
    //vkDeviceWaitIdle(device);

    destroy_swap_chain();

    if (!create_swap_chain())  return false;
    if (!create_image_views()) return false;
    
    return true;
}

bool Render_Backend::begin_frame() {
    vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &in_flight_fences[current_frame]);
    
    VkResult result = vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        logprintf("Failed to acquire swap chain image!");
        return false;
    }
    
    return true;
}

bool Render_Backend::end_frame() {
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {image_available_semaphores[current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = ArrayCount(wait_semaphores);
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers[current_frame];

    VkSemaphore signal_semaphores[] = {render_finished_semaphores[image_index]};
    submit_info.signalSemaphoreCount = ArrayCount(signal_semaphores);
    submit_info.pSignalSemaphores = signal_semaphores;

    if (vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences[current_frame]) != VK_SUCCESS) {
        logprintf("Failed to submit draw command buffer!\n");
        return false;
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = ArrayCount(signal_semaphores);
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swap_chains[] = {swap_chain};
    present_info.swapchainCount  = ArrayCount(swap_chains);
    present_info.pSwapchains     = swap_chains;
    present_info.pImageIndices   = &image_index;
    VkResult result = vkQueuePresentKHR(present_queue, &present_info);
    if (result != VK_SUCCESS) {
        logprintf("Failed to present swap chain image!\n");
        return false;
    }

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

    return true;
}

VkCommandBuffer Render_Backend::get_current_command_buffer(bool reset) {
    VkCommandBuffer result = command_buffers[current_frame];

    if (reset) {
        vkResetCommandBuffer(result, 0);
    }

    return result;
}

VkImage Render_Backend::get_current_swap_chain_image() {
    return swap_chain_images[image_index];
}

VkImageView Render_Backend::get_current_swap_chain_image_view() {
    return swap_chain_image_views[image_index];
}


// Copy-paste from: https://github.com/KhronosGroup/Vulkan-Samples/blob/main/framework/common/vk_common.cpp
void Render_Backend::image_layout_transition(VkCommandBuffer buffer, VkImage image, VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask, VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask, VkImageLayout old_layout, VkImageLayout new_layout, VkImageSubresourceRange subresource_range) {
    VkImageMemoryBarrier image_memory_barrier = {};

    image_memory_barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.srcAccessMask       = src_access_mask;
    image_memory_barrier.dstAccessMask       = dst_access_mask;
    image_memory_barrier.oldLayout           = old_layout;
    image_memory_barrier.newLayout           = new_layout;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image               = image;
    image_memory_barrier.subresourceRange    = subresource_range;

    vkCmdPipelineBarrier(buffer, src_stage_mask, dst_stage_mask, 0, 0, 0, 0, 0, 1, &image_memory_barrier);
}

static VkPipelineStageFlags get_pipeline_stage_flags(VkImageLayout layout) {
    switch (layout) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			return VK_PIPELINE_STAGE_HOST_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
			return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
			return VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		case VK_IMAGE_LAYOUT_GENERAL:
			Assert(!"Don't know how to get a meaningful VkPipelineStageFlags for VK_IMAGE_LAYOUT_GENERAL! Don't use it!");
			return 0;
	}

    Assert(false);
    return 0;
}

static VkAccessFlags get_access_flags(VkImageLayout layout) {
    switch (layout) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			return 0;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			return VK_ACCESS_HOST_WRITE_BIT;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
			return VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_ACCESS_TRANSFER_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		case VK_IMAGE_LAYOUT_GENERAL:
			Assert(!"Don't know how to get a meaningful VkAccessFlags for VK_IMAGE_LAYOUT_GENERAL! Don't use it!");
			return 0;
	}

    Assert(false);
    return 0;
}

void Render_Backend::image_layout_transition(VkCommandBuffer buffer, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkImageSubresourceRange subresource_range) {
    VkPipelineStageFlags src_stage_mask  = get_pipeline_stage_flags(old_layout);
    VkPipelineStageFlags dst_stage_mask  = get_pipeline_stage_flags(new_layout);
    VkPipelineStageFlags src_access_mask = get_access_flags(old_layout);
    VkPipelineStageFlags dst_access_mask = get_access_flags(new_layout);

    image_layout_transition(buffer, image, src_stage_mask, dst_stage_mask, src_access_mask, dst_access_mask, old_layout, new_layout, subresource_range);
}

bool Render_Backend::create_vertex_buffer(Gpu_Buffer *buffer, VkDeviceSize size, void *initial_data) {
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size  = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo buffer_allocation_create_info = {};
    buffer_allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    buffer_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateBuffer(allocator, &buffer_create_info, &buffer_allocation_create_info, &buffer->buffer, &buffer->allocation, NULL) != VK_SUCCESS) {
        logprintf("Failed to allocate vulkan vertex buffer!\n");
        return false;
    }

    buffer->size = size;

    void *mapped_buffer_data = NULL;
    if (vmaMapMemory(allocator, buffer->allocation, &mapped_buffer_data)) {
        logprintf("Failed to map vulkan buffer!\n");
        return false;
    }
    memcpy(mapped_buffer_data, initial_data, size);
    vmaUnmapMemory(allocator, buffer->allocation);

    return true;
}

bool Render_Backend::create_index_buffer(Gpu_Buffer *buffer, VkDeviceSize size, void *initial_data) {
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size  = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    VmaAllocationCreateInfo buffer_allocation_create_info = {};
    buffer_allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    buffer_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateBuffer(allocator, &buffer_create_info, &buffer_allocation_create_info, &buffer->buffer, &buffer->allocation, NULL) != VK_SUCCESS) {
        logprintf("Failed to allocate vulkan index buffer!\n");
        return false;
    }

    buffer->size = size;

    void *mapped_buffer_data = NULL;
    if (vmaMapMemory(allocator, buffer->allocation, &mapped_buffer_data)) {
        logprintf("Failed to map index buffer!\n");
        return false;
    }
    memcpy(mapped_buffer_data, initial_data, size);
    vmaUnmapMemory(allocator, buffer->allocation);

    return true;
}

bool Render_Backend::create_uniform_buffer(Gpu_Buffer *buffer, VkDeviceSize size, void *initial_data) {
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size  = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VmaAllocationCreateInfo buffer_allocation_create_info = {};
    buffer_allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    buffer_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateBuffer(allocator, &buffer_create_info, &buffer_allocation_create_info, &buffer->buffer, &buffer->allocation, NULL) != VK_SUCCESS) {
        logprintf("Failed to allocate vulkan uniform buffer!\n");
        return false;
    }

    buffer->size = size;

    if (initial_data) {
        void *mapped_buffer_data = NULL;
        if (vmaMapMemory(allocator, buffer->allocation, &mapped_buffer_data)) {
            logprintf("Failed to map uniform buffer!\n");
            return false;
        }
        memcpy(mapped_buffer_data, initial_data, size);
        vmaUnmapMemory(allocator, buffer->allocation);
    }
        
    return true;
}

bool Render_Backend::update_buffer(Gpu_Buffer *buffer, VkDeviceSize size, void *data) {
    void *mapped_buffer_data = NULL;
    if (vmaMapMemory(allocator, buffer->allocation, &mapped_buffer_data)) {
        logprintf("Failed to map uniform buffer!\n");
        return false;
    }
    memcpy(mapped_buffer_data, data, size);
    vmaUnmapMemory(allocator, buffer->allocation);

    return true;
}

void Render_Backend::destroy_buffer(Gpu_Buffer *buffer) {
    if (buffer->allocation) {
        vmaFreeMemory(allocator, buffer->allocation);
    }
    
    if (buffer->allocation) {
        vkDestroyBuffer(device, buffer->buffer, NULL);
    }
}

bool Render_Backend::create_descriptor_set_layout(VkDescriptorSetLayout *layout, int num_descriptor_layout_bindings, VkDescriptorSetLayoutBinding *descriptor_layout_bindings) {
    VkDescriptorSetLayoutCreateInfo descriptor_layout_create_info = {};
    descriptor_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout_create_info.bindingCount = num_descriptor_layout_bindings;
    descriptor_layout_create_info.pBindings = descriptor_layout_bindings;

    if (vkCreateDescriptorSetLayout(device, &descriptor_layout_create_info, NULL, layout) != VK_SUCCESS) {
        logprintf("Failed to create descriptor set layout!\n");
        return false;
    }

    return true;
}

bool Render_Backend::create_descriptor_pool(VkDescriptorPool *pool, int num_pool_sizes, VkDescriptorPoolSize *pool_sizes, int max_descriptor_sets) {
    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {};
    descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_create_info.maxSets       = max_descriptor_sets;
    descriptor_pool_create_info.poolSizeCount = num_pool_sizes;
    descriptor_pool_create_info.pPoolSizes    = pool_sizes;

    if (vkCreateDescriptorPool(device, &descriptor_pool_create_info, NULL, pool) != VK_SUCCESS) {
        logprintf("Failed to create descriptor pool!\n");
        return false;
    }

    return true;
}

bool Render_Backend::create_descriptor_sets(VkDescriptorPool descriptor_pool, VkDescriptorSetLayout layout, int num_descriptor_sets, VkDescriptorSet *descriptor_sets) {
    VkDescriptorSetLayout *descriptor_set_layouts = new VkDescriptorSetLayout[num_descriptor_sets];
    defer { delete [] descriptor_set_layouts; };
    for (int i = 0; i < num_descriptor_sets; i++) {
        descriptor_set_layouts[i] = layout;
    }
    
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
    descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_allocate_info.descriptorPool = descriptor_pool;
    descriptor_set_allocate_info.descriptorSetCount = num_descriptor_sets;
    descriptor_set_allocate_info.pSetLayouts = descriptor_set_layouts;

    if (vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, descriptor_sets) != VK_SUCCESS) {
        logprintf("Failed to allocate %d descriptor sets\n", num_descriptor_sets);
        return false;
    }

    return true;
}

void Render_Backend::update_descriptor_set(VkDescriptorSet set, int binding_slot, Gpu_Buffer *buffer) {
    VkDescriptorBufferInfo buffer_info = {};
    buffer_info.buffer = buffer->buffer;
    buffer_info.offset = 0;
    buffer_info.range  = buffer->size;
    
    VkWriteDescriptorSet wds = {};
    wds.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wds.dstSet          = set;
    wds.dstBinding      = binding_slot;
    wds.dstArrayElement = 0;
    wds.descriptorCount = 1;
    wds.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    wds.pBufferInfo     = &buffer_info;

    vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);
}

void Render_Backend::update_descriptor_set(VkDescriptorSet set, int binding_slot, Texture *texture) {
    VkDescriptorImageInfo image_info = {};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView   = texture->view;
    image_info.sampler     = texture->sampler;
    
    VkWriteDescriptorSet wds = {};
    wds.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wds.dstSet          = set;
    wds.dstBinding      = binding_slot;
    wds.dstArrayElement = 0;
    wds.descriptorCount = 1;
    wds.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wds.pImageInfo      = &image_info;

    vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);    
}

bool Render_Backend::load_texture(Texture *texture, char *filepath) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(1);
    stbi_uc *data = stbi_load(filepath, &width, &height, &channels, 4);
    if (!data) {
        logprintf("Failed to load image '%s'\n", filepath);
        return false;
    }
    defer { stbi_image_free(data); };

    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    
    return create_texture(texture, width, height, format, data, filepath);
}

bool Render_Backend::create_depth_buffer(Texture *texture, int width, int height, VkFormat format) {
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width  = width;
    image_info.extent.height = height;
    image_info.extent.depth  = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(allocator, &image_info, &alloc_info, &texture->image, &texture->allocation, NULL);

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = texture->image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;

    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    vkCreateImageView(device, &view_info, nullptr, &texture->view);

    texture->width  = width;
    texture->height = height;
    
    return true;
}

bool Render_Backend::create_texture(Texture *texture, int width, int height, VkFormat format, u8 *data, char *filepath) {
    VkImageCreateInfo image_create_info = {};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = format;
    image_create_info.extent.width = width;
    image_create_info.extent.height = height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(allocator, &image_create_info, &allocation_create_info, &texture->image, &texture->allocation, NULL) != VK_SUCCESS) {
        logprintf("Failed to create vulkan image!\n");
        return false;
    }

    VkDeviceSize layer_size = (VkDeviceSize)width * (VkDeviceSize)height * (VkDeviceSize)4;
    int layer_count = 1;
    VkDeviceSize image_size = layer_count * layer_size;

    Gpu_Buffer staging_buffer = {};
    
    VkBufferCreateInfo staging_buffer_create_info = {};
    staging_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_create_info.size  = image_size;
    staging_buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo staging_buffer_allocation_create_info = {};
    staging_buffer_allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    staging_buffer_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateBuffer(allocator, &staging_buffer_create_info, &staging_buffer_allocation_create_info, &staging_buffer.buffer, &staging_buffer.allocation, NULL) != VK_SUCCESS) {
        logprintf("Failed to allocate vulkan vertex buffer!\n");
        return false;
    }

    void *mapped_staging_buffer_data = NULL;
    if (vmaMapMemory(allocator, staging_buffer.allocation, &mapped_staging_buffer_data)) {
        logprintf("Failed to map vulkan buffer!\n");
        return false;
    }
    memcpy(mapped_staging_buffer_data, data, (u32)image_size);
    vmaUnmapMemory(allocator, staging_buffer.allocation);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(copy_command_buffer, &begin_info) != VK_SUCCESS) {
        logprintf("Failed to begin command buffer");
        return false;
    }

    VkImageSubresourceRange subresource_range = {};
    subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource_range.levelCount = 1;
    subresource_range.layerCount = 1;
    
    // VK_IMAGE_LAYOUT_UNDEFINED, VK_IMGE_LAYOUT_TRANSFER_DST_OPTIMAL
    image_layout_transition(copy_command_buffer, texture->image, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresource_range);

    VkBufferImageCopy buffer_image_copy = {};
    buffer_image_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    buffer_image_copy.imageSubresource.layerCount = 1;
    buffer_image_copy.imageExtent.width = width;
    buffer_image_copy.imageExtent.height = height;
    buffer_image_copy.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(copy_command_buffer, staging_buffer.buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);

    // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    image_layout_transition(copy_command_buffer, texture->image, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresource_range);

    vkEndCommandBuffer(copy_command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &copy_command_buffer;
    
    if (vkQueueSubmit(graphics_queue, 1, &submit_info, NULL) != VK_SUCCESS) {
        logprintf("Failed to submit to graphics and present queue");
        return false;
    }
    
    vkQueueWaitIdle(graphics_queue);

    VkImageViewCreateInfo create_info = {};
    create_info.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image        = texture->image;
    create_info.viewType     = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format       = format;
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel   = 0;
    create_info.subresourceRange.levelCount     = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device, &create_info, 0, &texture->view) != VK_SUCCESS) {
        logprintf("Failed to create '%s' image view!\n", filepath);
        return false;
    }

    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.maxAnisotropy = 1;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    if (vkCreateSampler(device, &sampler_info, VK_NULL_HANDLE, &texture->sampler) != VK_SUCCESS) {
        logprintf("Failed to create '%s' texture sampler!\n", filepath);
        return false;
    }

    texture->width  = width;
    texture->height = height;
    
    return true;    
}

void Render_Backend::destroy_texture(Texture *texture) {
    if (texture->view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, texture->view, NULL);
        texture->view = VK_NULL_HANDLE;
    }

    if (texture->image != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, texture->image, texture->allocation);
        texture->image = VK_NULL_HANDLE;
    }

    if (texture->sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, texture->sampler, NULL);
        texture->sampler = VK_NULL_HANDLE;
    }
}
