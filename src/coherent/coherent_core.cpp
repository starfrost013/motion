/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.

    coherent_core.cpp: Coherent Core code
*/

#include <coherent/coherent.hpp>

namespace Motion
{
    //
    // Cvars
    //
    Cvar* startPaused;


    //
    // Coherent class
    //

    /// @brief a C trampoline. since the logging system is portable across projects which may use various programming paradigms, it has to us lowest common 
    /// denominator for pointing to a function (ie a C-style function pointer.)
    /// @param str 
    void Coherent_CTrampolineForLog(const char* str)
    {
        CoherentUI::AddTextToLogWindowBuffer(str);
    }

    void Coherent::Init()
    {
        startPaused = Cvar::Set("startPaused", "1");

        Logger::Log(COHERENT_LOG_PREFIX, COHERENT_VERSION " initialised");
        Logger::settings.SetPostLogFunction(Coherent_CTrampolineForLog);
        CoherentUI::InitStyle(CoherentUI::UIStyle::Default);
        initialised = true; 
    }

    void Coherent::Enter()
    {
        active = true;
    }

    void Coherent::Tick()
    {
        if (breakpoints.size() == 0)
            return;

        // Check for guards

        for (auto& [ key, value ] : breakpoints)
        {
            auto pc = currentSystem->GetPC();

            if (pc == value.addr
            && value.enabled
            && !value.active) // no point repeatedly pausing
            {
                value.active = true; 

                // pause the emulation
                currentSystem->SetRunState(CoherentSystem::RunState::Paused);
            }
            else if (value.active && pc != value.addr)
                value.active = false;
        }
    }

    void Coherent::Leave()
    {   
        // shuts down session specific data
        breakpoints.clear();
        watchpoints.clear();

        extensions.clear();
        active = false; 
    }

    void Coherent::Shutdown()
    {
        currentSystem->Shutdown();
        Logger::settings.SetPostLogFunction(nullptr);
    }

    //
    // Extension registration
    //

    void Coherent::RegisterExtension(CoherentExtension* extension)
    {
        if (!extension)
        {
            Logger::Log(COHERENT_LOG_PREFIX, "Coherent::RegisterExtension - extension == NULLPTR!", LogChannels::Error);
            return; 
        }

        if (!extension->component)
        {
            Logger::Log(COHERENT_LOG_PREFIX, "Coherent::RegisterExtension - extension->component == NULLPTR!", LogChannels::Error);
            return; 
        }

        extensions.push_back(extension);
        Logger::Log(COHERENT_LOG_PREFIX, std::format("Coherent registered an extension for the component {}", extension->component->GetName()).c_str(), LogChannels::Debug);
    }

    //
    // Debug system
    //

    /// @brief Called when the coherent system was requested to add a breakpoint.
    void Coherent::AddBreakpoint(Breakpoint bp)
    {
        if (breakpoints.count(bp.addr) > 0)
        {
            Logger::Log(COHERENT_LOG_PREFIX, "Coherent::AddBreakpoint - the breakpoint already exists!", LogChannels::Warning);
            return; 
        }

        breakpoints[bp.addr] = bp;
    }

    /// @brief Called when the coherent system was requested to add a watchpoint.
    void Coherent::AddWatchpoint(Watchpoint wp)
    {
        if (watchpoints.count(wp.addr) > 0)
        {
            Logger::Log(COHERENT_LOG_PREFIX, "Coherent::AddWatchpoint - the watchpoint already exists!", LogChannels::Warning);
            return; 
        }
        
        watchpoints[wp.addr] = wp;
    }

    /// @brief Called when the coherent system was requested to remove a breakpoint.
    void Coherent::RemoveBreakpoint(Breakpoint bp)
    {
        if (breakpoints.count(bp.addr) > 0)
            breakpoints.erase(bp.addr);
        else
            Logger::Log(COHERENT_LOG_PREFIX, "Coherent::RemoveBreakpoint - the breakpoint doesn't exist!", LogChannels::Warning);
    }

    void Coherent::RemoveWatchpoint(Watchpoint wp)
    {
        if (watchpoints.count(wp.addr) > 0)
            watchpoints.erase(wp.addr);
        else
            Logger::Log(COHERENT_LOG_PREFIX, "Coherent::RemoveWatchpoint - the watchpoint doesn't exist!", LogChannels::Warning);
    }

    Coherent::Breakpoint Coherent::GetBreakpointByAddr(size_t addr)
    {
        return breakpoints[addr];
    }

    Coherent::Watchpoint Coherent::GetWatchpointByAddr(size_t addr)
    {
        return watchpoints[addr];
    }

    void Coherent::Exception(uint32_t exception)
    {
        if (breakOnException)
        {
            Logger::Log(COHERENT_LOG_PREFIX, std::format("Broke on exception 0x{:x}!!!", exception).c_str());
            currentSystem->SetRunState(CoherentSystem::RunState::Paused);
        }
    }     
}