#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <iostream>
#include <Windows.h>
#include <wtypes.h>
#include "platform.h"
#define VK_CHECK(result)                \
    if(result!=VK_SUCCESS){             \
        std::cout<<result<<std::endl;   \
        __debugbreak();                 \
        return false;                   \
    }

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
    VkDebugUtilsMessageTypeFlagsEXT msgFlags,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData
){
    std::cout<<"Validation error: "<<pCallbackData->pMessage<<std::endl;
    return false;
}

struct VkContext{
    VkExtent2D screenSize;

    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkRenderPass renderpass;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceFormatKHR surfaceFormat;

    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    int graphicsIdx=-1;

    VkQueue graphicsQueue;
    VkCommandPool commandPool;

    VkSemaphore aquireSemaphore;
    VkSemaphore submitSemaphore;


    uint32_t scImageCount;
    VkImage scImages[5];
    VkImageView scImageViews[5];
    VkFramebuffer framebuffers[5];
};

bool vk_init(VkContext *vkcontext, void *window){

    platform_get_window_size(&vkcontext->screenSize.width,&vkcontext->screenSize.height);

    VkApplicationInfo appInfo={};
    appInfo.apiVersion=VK_API_VERSION_1_3;
    appInfo.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName="pong";
    appInfo.pEngineName="PongEngine";

    char *extensions[]={
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME
    };

    char *layers[]={
        "VK_LAYER_KHRONOS_validation"
    };

    VkInstanceCreateInfo info={};
    info.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo=&appInfo;
    info.ppEnabledExtensionNames=extensions;
    info.enabledExtensionCount=sizeof(extensions)/sizeof(extensions[0]);
    info.ppEnabledLayerNames=layers;
    info.enabledLayerCount=sizeof(layers)/sizeof(layers[0]);

    VK_CHECK(vkCreateInstance(&info,0,&vkcontext->instance));


    auto vkCreateDebugUtilsMessengerEXT= (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkcontext->instance,"vkCreateDebugUtilsMessengerEXT");

    if(vkCreateDebugUtilsMessengerEXT){

        VkDebugUtilsMessengerCreateInfoEXT debugInfo={};
        debugInfo.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debugInfo.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugInfo.pfnUserCallback=vk_debug_callback;

        vkCreateDebugUtilsMessengerEXT(vkcontext->instance,&debugInfo,0,&vkcontext->debugMessenger);
    }else{
        return false;
    }

    //Create surface
    {
        VkWin32SurfaceCreateInfoKHR surfaceInfo={};
        surfaceInfo.sType=VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hwnd=(HWND)window;
        surfaceInfo.hinstance=GetModuleHandleA(0);
        VK_CHECK(vkCreateWin32SurfaceKHR(vkcontext->instance,&surfaceInfo,0,&vkcontext->surface)); 
    }

    //choose GPU
    {
        uint32_t gpuCount;
        VkPhysicalDevice gpus[10];

        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance,&gpuCount,0));
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance,&gpuCount,gpus));

        for(uint32_t i=0;i<gpuCount;i++){
            VkPhysicalDevice gpu=gpus[i];
            uint32_t queueFamilyCount=0;
            VkQueueFamilyProperties queueProps[10];
            vkGetPhysicalDeviceQueueFamilyProperties(gpu,&queueFamilyCount,0);
            vkGetPhysicalDeviceQueueFamilyProperties(gpu,&queueFamilyCount,queueProps);
            
            for (uint32_t j = 0; j < queueFamilyCount; j++)
            {
                if(queueProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT){
                    VkBool32 surfaceSupport=VK_FALSE;
                    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j,vkcontext->surface,&surfaceSupport));

                    if(surfaceSupport){
                        vkcontext->graphicsIdx=j;
                        vkcontext->gpu=gpu;
                        break;
                    }

                    
                }
            }
            
        }

        if(vkcontext->graphicsIdx<0){
            return false;
        }
    }

    //logical device
    {
        float queuePriority=1.0f;
        VkDeviceQueueCreateInfo queueInfo={};
        queueInfo.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex=vkcontext->graphicsIdx;
        queueInfo.queueCount=1;
        queueInfo.pQueuePriorities= &queuePriority;

        char *extensions[]={
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkDeviceCreateInfo deviceInfo={};
        deviceInfo.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pQueueCreateInfos=&queueInfo;
        deviceInfo.queueCreateInfoCount=1;
        deviceInfo.ppEnabledExtensionNames=extensions;
        deviceInfo.enabledExtensionCount=sizeof(extensions)/sizeof(extensions[0]);


        VK_CHECK(vkCreateDevice(vkcontext->gpu,&deviceInfo,0,&vkcontext->device));

        vkGetDeviceQueue(vkcontext->device,vkcontext->graphicsIdx,0,&vkcontext->graphicsQueue);
    }

    //Swapchain
    {
        uint32_t formatCount=0;
        //TODO: suballoc
        VkSurfaceFormatKHR surfaceFormats[10];
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu,vkcontext->surface,&formatCount,0));
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu,vkcontext->surface,&formatCount,surfaceFormats));

        for (uint32_t i = 0; i < formatCount; i++)
        {
            auto format=surfaceFormats[i];
            if(format.format==VK_FORMAT_B8G8R8A8_SRGB){
                vkcontext->surfaceFormat=format;
                break;
            }
        }
        

        VkSurfaceCapabilitiesKHR surfaceCaps={};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkcontext->gpu,vkcontext->surface,&surfaceCaps));

        uint32_t imgCount=0;
        imgCount=surfaceCaps.minImageCount+1;
        imgCount=imgCount>surfaceCaps.maxImageCount?imgCount-1:imgCount;

        VkSwapchainCreateInfoKHR scInfo={};
        scInfo.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scInfo.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        scInfo.surface=vkcontext->surface;
        scInfo.preTransform=surfaceCaps.currentTransform;
        scInfo.imageExtent=surfaceCaps.currentExtent;
        scInfo.minImageCount=imgCount;
        scInfo.imageFormat=vkcontext->surfaceFormat.format;
        scInfo.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scInfo.imageArrayLayers=1;

        VK_CHECK(vkCreateSwapchainKHR(vkcontext->device,&scInfo,0,&vkcontext->swapchain));


        VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device,vkcontext->swapchain,&vkcontext->scImageCount,0));
        VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device,vkcontext->swapchain,&vkcontext->scImageCount,vkcontext->scImages));

        //create image views
        {
            VkImageViewCreateInfo viewInfo={};
            viewInfo.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.format=vkcontext->surfaceFormat.format;
            viewInfo.viewType=VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.layerCount=1;
            viewInfo.subresourceRange.levelCount=1;
            
            for (uint32_t i = 0; i < vkcontext->scImageCount; i++)
            {
                viewInfo.image=vkcontext->scImages[i];
                VK_CHECK(vkCreateImageView(vkcontext->device,&viewInfo,0,&vkcontext->scImageViews[i]));
            }
        }
    }
    

    //Renderpass
    {
        VkAttachmentDescription attachment={};
        attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachment.samples=VK_SAMPLE_COUNT_1_BIT;
        attachment.format=vkcontext->surfaceFormat.format;

        VkAttachmentReference colorAttachmentRef={};
        colorAttachmentRef.attachment=0;
        colorAttachmentRef.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDesc={};
        subpassDesc.colorAttachmentCount=1;
        subpassDesc.pColorAttachments=&colorAttachmentRef;

        VkAttachmentDescription attachments[]={
            attachment
        };


        VkRenderPassCreateInfo rpInfo={};
        rpInfo.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.pAttachments=attachments;
        rpInfo.attachmentCount=sizeof(attachments)/sizeof(attachments[0]);
        rpInfo.subpassCount=1;
        rpInfo.pSubpasses=&subpassDesc;

        VK_CHECK(vkCreateRenderPass(vkcontext->device,&rpInfo,0,&vkcontext->renderpass));
    }

    //frame buffer
    {
        VkFramebufferCreateInfo fbInfo={};
        fbInfo.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.width=vkcontext->screenSize.width;
        fbInfo.height=vkcontext->screenSize.height;
        fbInfo.renderPass=vkcontext->renderpass;
        fbInfo.layers=1;
        fbInfo.attachmentCount=1;

        for (uint32_t i = 0; i < vkcontext->scImageCount; i++)
        {
            fbInfo.pAttachments=&vkcontext->scImageViews[i];
             VK_CHECK(vkCreateFramebuffer(vkcontext->device,&fbInfo,0,&vkcontext->framebuffers[i]));
        }
        
      
    }

    //Layout
    {
        VkPipelineLayoutCreateInfo layoutInfo={};
        layoutInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        VK_CHECK(vkCreatePipelineLayout(vkcontext->device,&layoutInfo,0,&vkcontext->pipelineLayout));
    }

    //Pipeline
    {
        VkShaderModule vertexShader,fragmentShader;

        uint32_t sizeInBytes;
        char *code=platform_read_file("assets/shaders/shader.vert.spv",&sizeInBytes);

        VkShaderModuleCreateInfo shaderInfo={};
        shaderInfo.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.pCode=(uint32_t *)code;
        shaderInfo.codeSize=sizeInBytes;

        VK_CHECK(vkCreateShaderModule(vkcontext->device,&shaderInfo,0,&vertexShader));
        delete code;

        code=platform_read_file("assets/shaders/shader.frag.spv",&sizeInBytes);
        shaderInfo.pCode=(uint32_t *)code;
        shaderInfo.codeSize=sizeInBytes;
        VK_CHECK(vkCreateShaderModule(vkcontext->device,&shaderInfo,0,&fragmentShader));
        delete code;

        VkPipelineShaderStageCreateInfo vertexStage={};
        vertexStage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexStage.stage=VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.pName="main";//entry point of vertex shader
        vertexStage.module=vertexShader;

        VkPipelineShaderStageCreateInfo fragmentStage={};
        fragmentStage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentStage.stage=VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.pName="main";//entry point of fragment shader
        fragmentStage.module=fragmentShader;
        
        VkPipelineShaderStageCreateInfo shaderStages[]={
            vertexStage,
            fragmentStage
        };


        VkPipelineVertexInputStateCreateInfo vertexInputState={};
        vertexInputState.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;



        //currently no blending
        VkPipelineColorBlendAttachmentState colorAttachment={};
        colorAttachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
        colorAttachment.blendEnable=VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlendState={};
        colorBlendState.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.pAttachments=&colorAttachment;
        colorBlendState.attachmentCount=1;

        VkPipelineRasterizationStateCreateInfo rasterizationState={};
        rasterizationState.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationState.frontFace=VK_FRONT_FACE_CLOCKWISE;
        rasterizationState.cullMode=VK_CULL_MODE_BACK_BIT;
        rasterizationState.polygonMode=VK_POLYGON_MODE_FILL;
        rasterizationState.lineWidth=1.0f;

        VkPipelineMultisampleStateCreateInfo multiSampleState={};
        multiSampleState.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multiSampleState.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly={};
        inputAssembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkRect2D scissor={};
        VkViewport viewport={};

        VkPipelineViewportStateCreateInfo viewportState={};
        viewportState.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.scissorCount=1;
        viewportState.viewportCount=1;
        viewportState.pScissors=&scissor;
        viewportState.pViewports=&viewport;

        VkDynamicState dynamicStates[]={
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState={};
        dynamicState.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount=sizeof(dynamicStates)/sizeof(dynamicStates[0]);
        dynamicState.pDynamicStates=dynamicStates;


        VkGraphicsPipelineCreateInfo pipeInfo={};
        pipeInfo.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeInfo.pVertexInputState=&vertexInputState;
        pipeInfo.pColorBlendState=&colorBlendState;
        pipeInfo.pStages=shaderStages;
        pipeInfo.stageCount=sizeof(shaderStages)/sizeof(shaderStages[0]);
        pipeInfo.pRasterizationState=&rasterizationState;
        pipeInfo.layout=vkcontext->pipelineLayout;
        pipeInfo.renderPass=vkcontext->renderpass;
        pipeInfo.pViewportState=&viewportState;
        pipeInfo.pDynamicState=&dynamicState;
        pipeInfo.pMultisampleState=&multiSampleState;
        pipeInfo.pInputAssemblyState=&inputAssembly;

        VK_CHECK(vkCreateGraphicsPipelines(vkcontext->device,0,1,&pipeInfo,0,&vkcontext->pipeline));
    }

    //Command pool
    {
        VkCommandPoolCreateInfo poolInfo={};
        poolInfo.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex=vkcontext->graphicsIdx;
        VK_CHECK(vkCreateCommandPool(vkcontext->device,&poolInfo,0,&vkcontext->commandPool));
    }

    //sync objs
    {
        VkSemaphoreCreateInfo semaphoreInfo={};
        semaphoreInfo.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        VK_CHECK(vkCreateSemaphore(vkcontext->device,&semaphoreInfo,0,&vkcontext->aquireSemaphore));
        VK_CHECK(vkCreateSemaphore(vkcontext->device,&semaphoreInfo,0,&vkcontext->submitSemaphore));
    }
    
    return true;
}

bool vk_render(VkContext *vkcontext){
    uint32_t imgIdx;
    VK_CHECK(vkAcquireNextImageKHR(vkcontext->device,vkcontext->swapchain,UINT64_MAX,vkcontext->aquireSemaphore,0,&imgIdx));

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocInfo={};
    allocInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandBufferCount=1;
    allocInfo.commandPool=vkcontext->commandPool;
    VK_CHECK(vkAllocateCommandBuffers(vkcontext->device,&allocInfo,&cmd));
    
    VkCommandBufferBeginInfo beginInfo={};
    beginInfo.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd,&beginInfo));

    VkClearValue clearValue={};
    clearValue.color={1,1,0,1};

    VkRenderPassBeginInfo rpBeginInfo={};
    rpBeginInfo.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass=vkcontext->renderpass;
    rpBeginInfo.renderArea.extent=vkcontext->screenSize;
    rpBeginInfo.framebuffer=vkcontext->framebuffers[imgIdx];
    rpBeginInfo.pClearValues=&clearValue;
    rpBeginInfo.clearValueCount=1;


    vkCmdBeginRenderPass(cmd,&rpBeginInfo,VK_SUBPASS_CONTENTS_INLINE);

    //rendering commands
    {
        VkRect2D scissor={};
        scissor.extent=vkcontext->screenSize;

        VkViewport viewport={};
        viewport.height=vkcontext->screenSize.height;
        viewport.width=vkcontext->screenSize.width;
        viewport.maxDepth=1.0f;

        vkCmdSetScissor(cmd,0,1,&scissor);
        vkCmdSetViewport(cmd,0,1,&viewport);

        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,vkcontext->pipeline);
        vkCmdDraw(cmd,3,1,0,0);
    }

    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSemaphore semaphores[]={
        vkcontext->submitSemaphore
    };
    
    VkSubmitInfo submitInfo={};
    submitInfo.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask=&waitStage;
    submitInfo.commandBufferCount=1;
    submitInfo.pCommandBuffers=&cmd;
    submitInfo.signalSemaphoreCount=1;
    submitInfo.pSignalSemaphores=semaphores;
    
    submitInfo.waitSemaphoreCount=1;
    submitInfo.pWaitSemaphores=&vkcontext->aquireSemaphore;

    VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue,1,&submitInfo,0));

    VkPresentInfoKHR presentInfo={};
    presentInfo.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains=&vkcontext->swapchain;
    presentInfo.swapchainCount=1;
    presentInfo.pImageIndices=&imgIdx;
    presentInfo.waitSemaphoreCount=1;
    presentInfo.pWaitSemaphores=semaphores;
    

    VK_CHECK(vkQueuePresentKHR(vkcontext->graphicsQueue,&presentInfo));

    VK_CHECK(vkDeviceWaitIdle(vkcontext->device));

    vkFreeCommandBuffers(vkcontext->device,vkcontext->commandPool,1,&cmd);

    return true;
}