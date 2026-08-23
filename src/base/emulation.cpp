/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    emulation.hpp: The implementation of the emulation thread
*/

#include <base/program.hpp>
#include <base/emulation.hpp>
#include <component/component.hpp>
#include <component/memory.hpp>
#include <coherent/coherent.hpp>
#include <coherent/coherent_editor.hpp>
#include <component/serial/serial.hpp>
#include <base/machine/machines.hpp>

#include <chrono>

namespace Motion
{    
    Cvar* machineName;

    void Emulation::Init()
    {
        InitMachine();
        Coherent::Init();
    }   

    /// @brief Determines the users machine type.
    void Emulation::InitMachine()
    {
        machineName = Cvar::Get("machineName", "iris3130");

        bool machineFound = false;

        if (!strcmp(machineName->GetString(), "iris3130"))
        {
            machineFound = true;
            machine = new IRIS3130();
        }

        if (!machineFound)
        {
            Logger::Log(std::format("Invalid machine {} selected. Defaulting to IRIS 3130...", machineName->GetString()).c_str(), LogChannels::Warning);
            machine = new IRIS3130();
        }

        // set the screen size to the size of the machine's framebuffer
        Program::GetRenderer()->SetScreenSize(Emulation::GetMachine()->GetInternalScreenSizeX(), Emulation::GetMachine()->GetInternalScreenSizeY());;

        Logger::Log(std::format("Adding components for machine {}...", machine->GetName()).c_str());
    }

    void Emulation::Start()
    {
        Logger::Log("Starting emulation...");

        machine->Start();
        // enter the coherent debugger
        Coherent::Enter();

        Logger::Log("Starting emulation: Starting emulation thread...", LogChannels::Debug);
        running = true; 

        // start the thread
        emuThread = new std::thread(Emulation::Tick);
    }
    
    /*
        dumpOnConsoleMatch catches the machine at a moment the guest announces. Plenty of the
        interesting ones it never mentions - a boot that simply goes quiet says nothing to match on -
        so this is the same idea on a stopwatch. Frame() rather than Tick() because this wants wall
        clock, and Tick() runs flat out.
    */
    int64_t Emulation::SecondsSinceStart()
    {
        static const auto start = std::chrono::steady_clock::now();

        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count();
    }

    void Emulation::CheckTimedDump()
    {
        if (!dumpAfterSeconds)
            dumpAfterSeconds = Cvar::Get("dumpAfterSeconds", "0");

        int32_t after = dumpAfterSeconds->GetValue();

        if (after <= 0 || timedDumpDone)
            return;

        if (SecondsSinceStart() < after)
            return;

        timedDumpDone = true;
        CoherentEditor::DumpAll(std::format("{} seconds elapsed", after).c_str());
    }

    /*
        Anything the guest only does in response to being typed at - a getty login, a shell, fsck
        asking whether to repair - is out of reach of a scripted run without this. consoleInput is
        typed at the console line starting consoleInputAfterSeconds in, which is late enough to let
        the kernel finish booting and userland open the line.

        \n, \r, \t, \\ and \xNN are understood, so a newline or a DEL survives a command line that
        would otherwise eat them. \pN waits N seconds before sending the rest, which is what makes
        a conversation possible: the answer to one prompt usually has to land before the next
        question is even asked.
    */
    void Emulation::CheckConsoleInput()
    {
        if (!consoleInput)
        {
            consoleInput = Cvar::Get("consoleInput", "");
            consoleInputAfterSeconds = Cvar::Get("consoleInputAfterSeconds", "0");
        }

        const char* text = consoleInput->GetString();
        int32_t after = consoleInputAfterSeconds->GetValue();

        if (after <= 0 || !text || !*text)
            return;

        // Parse once - the cvar is read every frame but only ever typed out the one time.
        static const std::vector<ConsoleInputChunk> chunks = ParseConsoleInput(text);

        if (consoleInputSent >= (int32_t)chunks.size())
            return;

        if (consoleInputNextSecond < 0)
            consoleInputNextSecond = after;

        if (SecondsSinceStart() < consoleInputNextSecond)
            return;

        ComponentSerial* serial = machine ? machine->FindComponentByType<ComponentSerial>() : nullptr;

        if (!serial)
            return;

        const ConsoleInputChunk& chunk = chunks[consoleInputSent];

        Logger::Log(std::format("consoleInput: typing {} bytes at serial line {}, then waiting {}s",
            chunk.text.size(), EMULATION_CONSOLE_SERIAL_LINE, chunk.pauseAfterSeconds).c_str());

        for (char c : chunk.text)
            serial->GetLine(EMULATION_CONSOLE_SERIAL_LINE).AddRxByte((uint8_t)c);

        consoleInputNextSecond = SecondsSinceStart() + chunk.pauseAfterSeconds;
        consoleInputSent++;
    }

    /// @brief Turn the consoleInput cvar into the chunks to type and how long to wait between them.
    std::vector<Emulation::ConsoleInputChunk> Emulation::ParseConsoleInput(const char* text)
    {
        std::vector<ConsoleInputChunk> chunks(1);

        for (const char* c = text; *c; c++)
        {
            if (*c != '\\' || !c[1])
            {
                chunks.back().text += *c;
                continue;
            }

            switch (*++c)
            {
                case 'n': chunks.back().text += '\n'; break;
                case 'r': chunks.back().text += '\r'; break;
                case 't': chunks.back().text += '\t'; break;
                case 'p':
                {
                    // \pN, N seconds. A bare \p waits the default.
                    int32_t seconds = EMULATION_CONSOLE_INPUT_DEFAULT_PAUSE;

                    if (isdigit((unsigned char)c[1]))
                        seconds = *++c - '0';

                    chunks.back().pauseAfterSeconds = seconds;
                    chunks.emplace_back();
                    break;
                }
                case 'x':
                {
                    // \xNN, and nothing else - a half written escape is likelier a typo than intent
                    if (isxdigit((unsigned char)c[1]) && isxdigit((unsigned char)c[2]))
                    {
                        chunks.back().text += (char)std::stoi(std::string(c + 1, c + 3), nullptr, 16);
                        c += 2;
                    }
                    break;
                }
                default: chunks.back().text += *c; break;
            }
        }

        return chunks;
    }

    void Emulation::Frame()
    {
        CheckTimedDump();
        CheckConsoleInput();

        //update our emulator event system
        Program::GetRenderer()->FramePreRender();

        if (Coherent::active)
            Coherent::Frame();
            
        Program::GetRenderer()->FramePostRender();
    }

    void Emulation::OnEvent(Event& evt)
    {
        machine->OnEvent(evt);
    }
     
    void Emulation::Reset()
    {
        Logger::Log("Resetting emulation...");

        Stop();
        Start();
    }

    void Emulation::SingleStep()
    {
        if (paused)
            machine->SingleStep();
    }

    void Emulation::Render(RenderTexture* screen)
    {
        machine->Render(screen);
    }

    void Emulation::Tick()
    {
        while (running)
        {
            // do this after processing everything for the current tick
            if (Coherent::active)
                Coherent::Tick();

            if (!paused)
                machine->Tick();
        }
    }

    void Emulation::Stop()
    {
        // Used by both Reset() (which immediately Start()s again) and Shutdown() (which doesn't), so the actual
        // "why" gets logged by whichever of those called us rather than here.

        // make sure the machine is joinable
        SetRunning(false);
        SetPaused(false);

        if (emuThread->joinable())
            emuThread->join();

        Coherent::Leave();
        machine->Shutdown();
        AddrSpace::Shutdown();
    }

    void Emulation::Shutdown()
    {
        Coherent::Shutdown();
        Stop();

        delete machine; 
    }
}