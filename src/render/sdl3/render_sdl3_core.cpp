/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    render_sdl3_core.cpp: Core SDL3 renderer stuff (init, shutdown, etc)
*/

#include <base/program.hpp>
#include <base/emulation.hpp>
#include <coherent/coherent.hpp>
#include <render/sdl3/render_sdl3.hpp>
#include <render/sdl3/render_sdl3_passes.hpp>

namespace Motion
{
    Cvar* vidScale;

    void RendererSDL3::DrawInitialDisplay()
    {
        SDL_Surface* defaultbg = SDL_LoadPNG("assets/defaultbg.png");

        if (defaultbg)
        {
            uint32_t* pixels = (uint32_t*)defaultbg->pixels;

            // should MEMCPY
            for (int32_t y = 0; y < 768; y++)
            {
                for (int32_t x = 0; x < 1024; x++)
                {
                    screen->SetPixel(x, y, pixels[(y * (defaultbg->pitch >> 2) + x)]);
                }
            }  
        }
        else
        {
            Logger::Log("Cool logo failed to load, no assets/defaultbg.png", LogChannels::Error);

            float red = 1.000;
            float blue = 0.000;
            float green = 0.000;
            
            for (int32_t y = 0; y < 768; y++)
            {
                for (int32_t x = 0; x < 1024; x++)
                {
                    screen->SetPixel(x, y, Color((uint8_t)(red * 256), (uint8_t)(green * 256), (uint8_t)(blue * 256), 255.0));
                    green += (1.0f/1024.0f);
                }

                blue += (1.0f/768.0f);
                red -= (1.0f/768.0f);
            }  
        }
    }

    /// @brief Initialises the SDL renderer. A failure is reported as a FATAL_ERROR Log.
    void RendererSDL3::Init()
    {
        Logger::Log(LOG_PREFIX_RENDER_SDL3, "Initialising SDL3...");

        
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
            Logger::Log(LOG_PREFIX_RENDER_SDL3, std::format("Failed to initialise SDL3: {}!", SDL_GetError()).c_str(), LogChannels::FatalError);


        // Content scaling, for 4k displays. Set with e.g. '+set vidScale 2' on the command line
        float scale = Cvar::Get("vidScale", "1")->GetValue();

        if (scale < 0.5f || scale > 4.0f)
        {
            Logger::Log(LOG_PREFIX_RENDER_SDL3, std::format("vidScale {} is out of range (0.5-4.0), clamping.", scale).c_str(), LogChannels::Warning);
            scale = std::clamp(scale, 0.5f, 4.0f);
        }

        window.SetWindowSize((int32_t)(WINDOW_DEFAULT_SIZE_X * scale), (int32_t)(WINDOW_DEFAULT_SIZE_Y * scale));
        window.Start();
    
        Logger::Log(LOG_PREFIX_RENDER_SDL3, "Initialising SDL GPU device...", LogChannels::Debug);

        gpuDevice = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_SPIRV 
            | SDL_GPU_SHADERFORMAT_DXIL
            | SDL_GPU_SHADERFORMAT_MSL
            | SDL_GPU_SHADERFORMAT_METALLIB,
        #ifdef DEBUG
            true,
        #else
            false,
        #endif
            nullptr // let sdl pick the optimal driver
        );

        if (!gpuDevice) // noreturn
            Logger::Log(LOG_PREFIX_RENDER_SDL3, std::format("Failed to initialise SDL GPU Device!", SDL_GetError()).c_str(), LogChannels::FatalError);

        Logger::Log(LOG_PREFIX_RENDER_SDL3, "Initialising SDL GPU renderer...", LogChannels::Debug);

        renderer = SDL_CreateGPURenderer(gpuDevice, window.GetInternalWindow());

        if (!renderer) // noreturn
            Logger::Log(LOG_PREFIX_RENDER_SDL3, std::format("Failed to initialise SDL GPU Renderer!", SDL_GetError()).c_str(), LogChannels::FatalError);

        Logger::Log(LOG_PREFIX_RENDER_SDL3, "Claiming window for GPU device...", LogChannels::Debug);

        // Claim the window for the gpu device
        if (!SDL_ClaimWindowForGPUDevice(gpuDevice, window.GetInternalWindow())) // noreturn
            Logger::Log(LOG_PREFIX_RENDER_SDL3, std::format("Failed to claim SDL window for GPU device!", SDL_GetError()).c_str(), LogChannels::FatalError);
 
        Logger::Log(LOG_PREFIX_RENDER_SDL3, "Setting swapchain parameters...", LogChannels::Debug);

        // Set SDR
        SDL_SetGPUSwapchainParameters(gpuDevice, window.GetInternalWindow(), SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);

        Logger::Log(LOG_PREFIX_RENDER_SDL3, "Initialising IMGUI core...", LogChannels::Debug);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // setup input
        io = &ImGui::GetIO();
        io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Keep fonts rendering crisp when content scaling is active
        ImFontConfig fontConfig = ImFontConfig();
        fontConfig.SizePixels = 13.0f * scale;
        io->Fonts->AddFontDefault(&fontConfig);

        // setup style
        ImGui::StyleColorsDark();

        Logger::Log(LOG_PREFIX_RENDER_SDL3, "Initialising IMGUI backend...", LogChannels::Debug);

        ImGui_ImplSDL3_InitForSDLGPU(window.GetInternalWindow());

        ImGui_ImplSDLGPU3_InitInfo initInfo = {};
        initInfo.Device = gpuDevice;
        initInfo.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpuDevice, window.GetInternalWindow());
        initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        initInfo.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        initInfo.PresentMode = SDL_GPU_PRESENTMODE_VSYNC; // VSync enabled? Make it a convar?

        ImGui_ImplSDLGPU3_Init(&initInfo);

        // create our screen texture. 
        // INitially it is the size of the window, but later on, we re-create it when the machine is selected.
        SetScreenSize(Program::GetRenderer()->GetWindowSizeX(), Program::GetRenderer()->GetWindowSizeY());

        // never goes out of scope, never deleted. lol!
        MainRenderPass* renderPass = new MainRenderPass();
        AddRenderPass(renderPass);


        // draw something so we know the renderer works
        DrawInitialDisplay();
    }

    /// @brief set the screen size
    /// @param x the x coordinate of the screen size to set
    /// @param y the y coordinate of the screen size to set
    void RendererSDL3::SetScreenSize(int32_t x, int32_t y) 
    {
        if (screen)
            delete screen;

        screen = new RenderTextureSDL3(this, x, y, RenderTextureDrawType::DrawAsWindowSize);

        // create our gpu transfer buffer
        CreateTransferBuffer();
    }

    void RendererSDL3::CreateTransferBuffer()
    {
        // recreate it if e.g. we changed screen size
        if (transfer)
            SDL_ReleaseGPUTransferBuffer(gpuDevice, transfer);

        SDL_GPUTransferBufferCreateInfo gpuXferInfo = SDL_GPUTransferBufferCreateInfo();

        gpuXferInfo.usage = SDL_GPUTransferBufferUsage::SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        gpuXferInfo.size = (screen->sizeX * screen->sizeY) << 2;

        transfer = SDL_CreateGPUTransferBuffer(gpuDevice, &gpuXferInfo);

        if (!transfer)
            Logger::Log(LOG_PREFIX_RENDER_SDL3, "Failed to create GPU transfer buffer ??", LogChannels::FatalError);
            
    }

    /// @brief Render a new frame.
    void RendererSDL3::FramePreRender()
    {
        SDL_Event event;

        // tell the event system about various things
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);

            // quit if we need to
            if (event.type == SDL_EVENT_QUIT)
                Program::running = false; 
                
            /*
                Losing focus has to be told to the emulation whether or not ImGui wants the keyboard,
                because the key-up for anything currently held is going to be delivered somewhere
                else entirely. A keyboard that tracks held keys - and the IRIS one has to, IRIX
                works out shift and control purely from make and break codes - would otherwise be
                left believing a modifier is still down.
            */
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
            {
                FocusLostEvent evt = FocusLostEvent();
                EventSystem::FireEvent(evt);
            }

            /*
                WantTextInput, not WantCaptureKeyboard. The latter is true for as long as ImGui has
                keyboard navigation active, which - with ImGuiConfigFlags_NavEnableKeyboard set
                above - is essentially always once any window has been focused. Gating on it meant
                every keystroke was swallowed before it reached the guest and the emulated keyboard
                looked dead.

                WantTextInput is the narrower question actually being asked here: is the user typing
                into a debugger text field right now? If they are, the debugger should have the keys.
                If they are not, the machine should.

                Note the flag still leaves ImGui reacting to the arrow keys, Enter and Space for its
                own navigation while the guest is also receiving them. Dropping
                ImGuiConfigFlags_NavEnableKeyboard would settle that, but that is a UI decision.
            */
            if (!io->WantTextInput)
            {
                if (event.type == SDL_EVENT_KEY_DOWN)
                {
                    // TEMP : some basic keyboard controls.
                    // need to figure out how our event system is going to work so we can have backend independent events
                    // maybe components can subscribe to events
                    switch (event.key.key)
                    {
                        case SDLK_F9:
                            Coherent::active = !Coherent::active;
                            break;
                    }

                    // tell the event system
                    KeyDownEvent evt = KeyDownEvent();
                    evt.key = event.key.key;
                    evt.scancode = event.key.scancode;
                    evt.mod = event.key.mod;
                    evt.repeat = event.key.repeat;
                    EventSystem::FireEvent(evt);
                }
                else if (event.type == SDL_EVENT_KEY_UP)
                {
                    KeyUpEvent evt = KeyUpEvent();
                    evt.key = event.key.key;
                    evt.scancode = event.key.scancode;
                    evt.mod = event.key.mod;
                    evt.repeat = event.key.repeat;
                    EventSystem::FireEvent(evt);
                }
            }

            if (!io->WantCaptureMouse)
            {
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                {
                    MouseDownEvent evt = MouseDownEvent();
                    evt.mouse = event.button.button;
                    evt.numClicks = event.button.clicks;
                    EventSystem::FireEvent(evt);
                }
                else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
                {
                    MouseUpEvent evt = MouseUpEvent();
                    evt.mouse = event.button.button;
                    evt.numClicks = event.button.clicks;
                    EventSystem::FireEvent(evt);
                }
                else if (event.type == SDL_EVENT_MOUSE_MOTION)
                {
                    /*
                        Relative, not absolute. The IRIS mouse is a quadrature encoder - it reports
                        that it moved one tick, and the direction, and nothing else - so where the
                        host pointer is on the host screen is not something the guest can be told.
                        The two pointers therefore drift apart whenever the host one leaves the
                        window or the guest one hits an edge, which is what a real machine sharing a
                        desk with the mouse would do too.
                    */
                    MouseMotionEvent evt = MouseMotionEvent();
                    evt.deltaX = event.motion.xrel;
                    evt.deltaY = event.motion.yrel;
                    EventSystem::FireEvent(evt);
                }
            }

        }

        // run the passes of the Imgui
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }


    void RendererSDL3::FramePostRender()
    {
        ImGui::Render();

        // Upload the IMGUI vertex and index buffers to the GPU
        // IMGUI is always on top of whatever the GPURenderer is rendereing

        ImDrawData* data = ImGui::GetDrawData();
        bool isMinimised = (data->DisplaySize.x < 0.0f | data->DisplaySize.y < 0.0f);
        commandBuffer = SDL_AcquireGPUCommandBuffer(gpuDevice);
        
        SDL_AcquireGPUSwapchainTexture(commandBuffer, window.GetInternalWindow(), &swapchainTexture, nullptr, nullptr);

        if (swapchainTexture && !isMinimised)
        {
            // THIS IS A TERRIBLE WAY OF DOING THIS
            if (Program::GetState() == ProgramState::Emulation)
            {
                // any component which needs to render now renders
                Emulation::Render(screen);
            }

            // run our render passes
            for (RenderPass* pass : passes)
                pass->Render(this, screen);

            // then render imgui

            ImGui_ImplSDLGPU3_PrepareDrawData(data, commandBuffer);

            SDL_GPUColorTargetInfo targetInfo = {};
            targetInfo.texture = swapchainTexture;
            targetInfo.clear_color = { 0.0f, 0.0f, 0.0f, 1.0f }; 
            targetInfo.load_op = SDL_GPU_LOADOP_LOAD; // allow IMGUI to layer on top of the emulator
            targetInfo.store_op = SDL_GPU_STOREOP_STORE;
            targetInfo.mip_level = 0; // 2D
            targetInfo.layer_or_depth_plane = 0; 
            targetInfo.cycle = false;

            SDL_GPURenderPass* imguiPass = SDL_BeginGPURenderPass(commandBuffer, &targetInfo, 1, nullptr); 
            ImGui_ImplSDLGPU3_RenderDrawData(data, commandBuffer, imguiPass);
            SDL_EndGPURenderPass(imguiPass);
        }
        
        SDL_SubmitGPUCommandBuffer(commandBuffer);
    }

    /// @brief Shut down the renderer.
    void RendererSDL3::Shutdown()
    {
        delete screen; 

        // Wait for the gpu to shut down so we don't corrupt its state
        SDL_WaitForGPUIdle(gpuDevice);

        for (RenderPass* pass : passes)
            delete pass; 

        passes.clear();

        // free the transfer buffer
        if (transfer)
            SDL_ReleaseGPUTransferBuffer(gpuDevice, transfer);

        ImGui_ImplSDLGPU3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        SDL_ReleaseWindowFromGPUDevice(gpuDevice, window.GetInternalWindow());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyGPUDevice(gpuDevice);

        window.Shutdown();
        
        SDL_Quit();
    }
}