#pragma once
#include <vulkan/vulkan.h>

VkSubmitInfo createSubmitInfo(VkCommandBuffer *cmd,uint32_t cmdCount=1){
    VkSubmitInfo submitInfo={};
    submitInfo.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount=cmdCount;
    submitInfo.pCommandBuffers=cmd;
    return submitInfo;    
}