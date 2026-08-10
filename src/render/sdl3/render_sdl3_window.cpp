/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    render_sdl3_window.cpp: WindowSDL3 class
*/

#include <render/sdl3/render_sdl3.hpp>

namespace Motion
{
    void WindowSDL3::Start()
    {
        Logger::Log(LOG_PREFIX_RENDER_SDL3, "Initialising SDL window...", LogChannels::Debug);

        window = SDL_CreateWindow(WINDOW_TITLE_DEFAULT, sizeX, sizeY, SDL_WINDOW_HIGH_PIXEL_DENSITY);

        if (!window) // noreturn
            Logger::Log(LOG_PREFIX_RENDER_SDL3, std::format("Failed to initialise SDL Window!", SDL_GetError()).c_str(), LogChannels::FatalError);
    } 

    void WindowSDL3::Shutdown() 
    {
        SDL_DestroyWindow(window);
    }

    // Allow resizing the window
    void WindowSDL3::SetWindowSize(int32_t x, int32_t y)
    {
        sizeX = x;
        sizeY = y;

        if (window)
            SDL_SetWindowSize(window, x, y);
    }

}
