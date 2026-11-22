#pragma once

#include <SDL.h>

#include <string>

class Framebuffer;

// 一帧里窗口告诉主循环的事：退出/存盘这类一次性事件，加上鼠标的增量输入。
struct FrameInput {
    bool quit = false;           // 关窗 或 按 ESC
    bool saveRequested = false;  // 按 S
    bool resetView = false;      // 按 R

    // 增量而不是绝对位置：主循环只关心"这一帧转了多少"，窗口多大都成立
    float mouseDX = 0.0f;        // 左键拖动位移，像素，向右为正
    float mouseDY = 0.0f;        // 向下为正
    float wheel = 0.0f;          // 滚轮格数，向前为正
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
