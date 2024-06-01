#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <iostream>
#include <Windows.h>
#include <wtypes.h>
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
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceFormatKHR surfaceFormat;
    int graphicsIdx=-1;

    VkQueue graphicsQueue;
    VkCommandPool commandPool;

    VkSemaphore aquireSemaphore;
    VkSemaphore submitSemaphore;


    uint32_t scImageCount;
    VkImage scImages[5];
};

bool vk_init(VkContext *vkcontext, void *window){
    VkApplicationInfo appInfo={};
    appInfo.apiVersion=VK_API_VERSION_1_0;
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
    VK_CHECK(vkAcquireNextImageKHR(vkcontext->device,vkcontext->swapchain,0,vkcontext->aquireSemaphore,0,&imgIdx));

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

    //rendering commands
    {
        VkClearColorValue color={1,1,0,1};
        VkImageSubresourceRange range={};

        range.layerCount=1;
        range.levelCount=1;
        range.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;

       vkCmdClearColorImage(cmd,vkcontext->scImages[imgIdx],VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,&color,1,&range);
    }

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    
    VkSubmitInfo submitInfo={};
    submitInfo.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask=&waitStage;
    submitInfo.commandBufferCount=1;
    submitInfo.pCommandBuffers=&cmd;
    submitInfo.pSignalSemaphores=&vkcontext->submitSemaphore;
    submitInfo.signalSemaphoreCount=1;
    submitInfo.waitSemaphoreCount=1;
    submitInfo.pWaitSemaphores=&vkcontext->aquireSemaphore;

    VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue,1,&submitInfo,0));

    VkPresentInfoKHR presentInfo={};
    presentInfo.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains=&vkcontext->swapchain;
    presentInfo.swapchainCount=1;
    presentInfo.pImageIndices=&imgIdx;
    presentInfo.pWaitSemaphores=&vkcontext->submitSemaphore;
    presentInfo.waitSemaphoreCount=1;

    VK_CHECK(vkQueuePresentKHR(vkcontext->graphicsQueue,&presentInfo));

    VK_CHECK(vkDeviceWaitIdle(vkcontext->device));

    vkFreeCommandBuffers(vkcontext->device,vkcontext->commandPool,1,&cmd);
}