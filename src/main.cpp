#include <iostream>
#include <vulkan/vulkan.h>
int main(){
    VkApplicationInfo appInfo={};
    appInfo.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName="pong";
    appInfo.pEngineName="PongEngine";

    VkInstanceCreateInfo info={};
    info.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo=&appInfo;
    VkInstance instance;
    VkResult result=vkCreateInstance(&info,0,&instance);
    if(result==VK_SUCCESS){
        std::cout<<"Successfully created vulkan instance\n";
    }
    return 0;
}