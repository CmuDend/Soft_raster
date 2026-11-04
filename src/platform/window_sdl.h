#pragma once

#include <SDL.h>

#include <string>

class Framebuffer;

// 一帧里窗口告诉主循环的事。D4 加鼠标（轨道相机）时往这里扩字段。
struct FrameInput {
    bool quit = false;           // 关窗 或 按 ESC
    bool saveRequested = false;  // 按 S
};

// SDL 只出现在这一层：开窗、收输入、把 framebuffer 贴成一张纹理。
// 渲染管线本身不 include 任何 SDL 头文件。
class Window {
public:
    Window(const char* title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    FrameInput pollEvents();
    void present(const Framebuffer& fb);
    void setTitle(const std::string& title);

private:
    void cleanup();

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_texture = nullptr;
    int m_width = 0;
    int m_height = 0;
    bool m_sdlReady = false;
};
