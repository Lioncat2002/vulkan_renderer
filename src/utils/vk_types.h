#pragma once

#include <vulkan/vulkan.h>

struct Image{
    VkImage image;
    VkDeviceMemory memory;

};

struct Buffer{
    VkBuffer buffer;
    VkDeviceMemory memory;
    void *data;
};