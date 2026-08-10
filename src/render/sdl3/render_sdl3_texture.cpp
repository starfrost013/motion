/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    render_sdl3_texture.cpp: IMplements 
*/

#include <render/render.hpp>
#include <render/sdl3/render_sdl3.hpp>

namespace Motion
{
    RenderTextureSDL3::RenderTextureSDL3(Renderer* renderer, uint32_t sizeX, uint32_t sizeY, RenderTextureDrawType drawType) 
    : RenderTextureSDL3::RenderTextureSDL3(renderer, sizeX, sizeY, sizeX, sizeY, sizeX, sizeY, drawType)
    {

    }

    RenderTextureSDL3::RenderTextureSDL3(Renderer* renderer, int32_t sizeX, int32_t sizeY, int32_t srcSizeX, int32_t srcSizeY, int32_t destSizeX, int32_t destSizeY, 
        RenderTextureDrawType drawType) : RenderTexture::RenderTexture(renderer, sizeX, sizeY, srcSizeX, srcSizeY, destSizeX, destSizeY, drawType)
    {
        this->renderer = renderer;

        RendererSDL3* sdl3Renderer = static_cast<RendererSDL3*>(renderer);

        // should be fine for now
        SDL_GPUTextureCreateInfo createInfo = SDL_GPUTextureCreateInfo();
        createInfo.num_levels = 1;
        createInfo.format = SDL_GPUTextureFormat::SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        createInfo.width = sizeX;
        createInfo.height = sizeY;
        createInfo.sample_count = SDL_GPUSampleCount::SDL_GPU_SAMPLECOUNT_1;
        createInfo.layer_count_or_depth = 1;
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

        Logger::Log(LOG_PREFIX_RENDER_SDL3, std::format("RenderTextureSDL3 constructor: creating a texture of size {} x {}", sizeX, sizeY).c_str(), LogChannels::Debug);

        texture = SDL_CreateGPUTexture(sdl3Renderer->gpuDevice, &createInfo);
    }

    // destructor

    RenderTextureSDL3::~RenderTextureSDL3()
    {
        RendererSDL3* sdl3Renderer = static_cast<RendererSDL3*>(renderer);
        SDL_ReleaseGPUTexture(sdl3Renderer->gpuDevice, texture);
        
        delete[] pixels; 
    }

};