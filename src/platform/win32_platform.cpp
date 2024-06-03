#include <iostream>
#include <Windows.h>
#include <wtypes.h>
#include "defines.h"
#include "platform.h"
#include "renderer/vk_renderer.cpp"

global_variable bool running =true;
global_variable HWND window=0;
LRESULT CALLBACK platform_window_callback(HWND window,UINT msg, WPARAM wParam,LPARAM lParam){
    
    switch(msg){
        case WM_CLOSE:
        running=false;
        break;
    }
    
    return DefWindowProcA(window,msg,wParam,lParam);
}

bool platform_create_window(){
    HINSTANCE instance=GetModuleHandleA(0);

    WNDCLASS wc={};
    wc.lpfnWndProc=platform_window_callback;
    wc.hInstance=instance;
    wc.lpszClassName="vulkan_engine_class";
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    if(!RegisterClassA(&wc)){
        MessageBoxA(window,"failed registering window class","Error",MB_ICONEXCLAMATION|MB_OK);
        return false;
    }

    window=CreateWindowA(
        "vulkan_engine_class",
        "Pong",
        WS_THICKFRAME|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_OVERLAPPED
        ,100,100,1280,720,0,0,instance,0
    );
    if(window==0){
        MessageBoxA(window,"failed creating window","error",MB_ICONEXCLAMATION|MB_OK);
        return false;
    }
    ShowWindow(window,SW_SHOW);
    return true;
}

void platform_update_window(){
    MSG msg;

    while(PeekMessageA(&msg,window,0,0,PM_REMOVE)){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}


void platform_get_window_size(uint32_t* width,uint32_t* height){
    RECT rect;
    GetClientRect(window,&rect);
    *width=rect.right-rect.left;
    *height=rect.bottom-rect.top;
}

char* platform_read_file(char *path, uint32_t *length){
    char* result=0;
    HANDLE file=CreateFile(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
    if(file!=INVALID_HANDLE_VALUE){
        LARGE_INTEGER size;
       if(GetFileSizeEx(file,&size)){

        *length=(uint32_t)size.QuadPart;
        result=new char[*length];

        DWORD bytesRead=0;
        if(ReadFile(file,result,*length,&bytesRead,NULL)){
            //success
        }else{
            std::cout<<"failed to read file: "<<path<<std::endl;
        }
       }else{
        std::cout<<"failed to get size of: "<<path<<std::endl;
       }
       CloseHandle(file);
    }else{
        //TODO: asset
        std::cout<<"failed to open file: "<<path<<std::endl;
    }
    return result;
}

int main(){

    VkContext vkcontext={};
    
    if(!platform_create_window()){
        return -1;

    }
    if(!vk_init(&vkcontext,window)){
        std::cout<<"failed to create vulkan context\n";
        return -1;
    }
    while(running){
        vk_render(&vkcontext);
        platform_update_window();
    }
    return 0;
}