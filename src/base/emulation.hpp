/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    emulation.hpp: The core of the emulation thread
*/

#pragma once
#include <Motion.hpp>
#include <base/filesystem/filesystem.hpp>
#include <base/machine/machine.hpp>
#include <base/event/event.hpp>

// render includes
#include <render/render.hpp>
#include <render/sdl3/render_sdl3.hpp>

namespace Motion
{
    // Temp until convars are a thi9ng
    #define CONFIG_PATH "Motion.cfg"
    
    extern Cvar* machineName;

    // The line the IRIS uses for its console. Line 0 is the other DUART channel.
    #define EMULATION_CONSOLE_SERIAL_LINE           1

    // How long a bare \p in consoleInput waits, in seconds.
    #define EMULATION_CONSOLE_INPUT_DEFAULT_PAUSE   5

    class Emulation
    {
    public: 
        static void Init();             // initialise the emulation system
        static void Start();            // Start emulation
        static void Frame();            // render a frame
        static void Tick();             // run one tick of the system
        static void Render(RenderTexture* screen);  // render
        static void OnEvent(Event& evt);   // fire an event
        static void Reset();            // reset the emulation
        static void SingleStep();       // run one emulation tick
        static void Stop();             // shut down
        static void Shutdown();     

        // Called by components to e.g. get the address space
        static Machine* GetMachine() { return machine; }
        
        static bool IsRunning() { return running; }    
        static void SetRunning(bool value) { running = value; };

        static bool GetPaused() { return paused; };

        // setters for private fields
        static void SetPaused(bool paused) { Emulation::paused = paused; };

    private: 
        /// @brief determines if the emulator is running
        inline static bool running = false;

        /// @brief determines if the emulator is paused
        inline static bool paused = false; 
        
        inline static FileStream config; 

        /// @brief the machine that is being emulated
        inline static Machine* machine;

        /// @brief the thread that the emulation runs on
        inline static std::thread* emuThread;

        /// @brief initialises the machine based on the convar values.
        inline static void InitMachine();

        /// @brief Wall clock seconds since the first time anything asked, i.e. roughly since startup.
        static int64_t SecondsSinceStart();

        /// @brief Write every memory editor out once dumpAfterSeconds have passed, if it is set.
        static void CheckTimedDump();

        /// @brief One run of text to type, and how long to wait afterwards before the next.
        struct ConsoleInputChunk
        {
            std::string text;
            int32_t pauseAfterSeconds = 0;
        };

        /// @brief Type consoleInput at the guest once consoleInputAfterSeconds have passed.
        static void CheckConsoleInput();

        static std::vector<ConsoleInputChunk> ParseConsoleInput(const char* text);

        inline static Cvar* dumpAfterSeconds = nullptr;
        inline static bool timedDumpDone = false;

        inline static Cvar* consoleInput = nullptr;
        inline static Cvar* consoleInputAfterSeconds = nullptr;
        inline static int32_t consoleInputSent = 0;
        inline static int64_t consoleInputNextSecond = -1;
    };
}