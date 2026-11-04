#include "platform/window_sdl.h"

#include <stdexcept>

#include "core/framebuffer.h"

namespace {
constexpr int kBytesPerPixel = sizeof(Color);
}  // namespace

Window::Window(const char* title, int width, int height) : m_width(width), m_height(height) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        throw std::runtime_error(std::string("SDL_Init 失败: ") + SDL_GetError());
    }
    m_sdlReady = true;

    m_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                                SDL_WINDOW_SHOWN);
    if (!m_window) throw std::runtime_error(std::string("创建窗口失败: ") + SDL_GetError());

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    if (!m_renderer) throw std::runtime_error(std::string("创建渲染器失败: ") + SDL_GetError());

    // STREAMING：每帧都会整块重传，这个 usage 提示驱动不要给我们做显存常驻优化。
    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  width, height);
    if (!m_texture) throw std::runtime_error(std::string("创建纹理失败: ") + SDL_GetError());
}

Window::~Window() { cleanup(); }

void Window::cleanup() {
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_sdlReady) {
        SDL_Quit();
        m_sdlReady = false;
    }
}

FrameInput Window::pollEvents() {
    FrameInput in;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            in.quit = true;
        } else if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE) in.quit = true;
            if (e.key.keysym.sym == SDLK_s) in.saveRequested = true;
        }
    }
    return in;
}

void Window::present(const Framebuffer& fb) {
    // 纹理尺寸是创建时定死的，帧缓冲换了尺寸就得重建纹理（D4 做窗口缩放再处理）
    if (fb.width() != m_width || fb.height() != m_height) return;

    // 整块上传：CPU 内存 -> 纹理，格式一致所以是一次 memcpy 级别的拷贝
    SDL_UpdateTexture(m_texture, nullptr, fb.colorData(), m_width * kBytesPerPixel);
    SDL_RenderClear(m_renderer);
    SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
    SDL_RenderPresent(m_renderer);
}

void Window::setTitle(const std::string& title) { SDL_SetWindowTitle(m_window, title.c_str()); }
