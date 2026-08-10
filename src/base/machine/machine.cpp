/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    machine.hpp: Implements the shared code between all machines.
*/

#include <base/machine/machine.hpp>
#include <coherent/coherent.hpp>

namespace Motion
{
    Cvar *ramInstalled;

    void Machine::Start()
    {
        // components are added earlier so the renderer can read the screen size
        AddComponents();

        ramInstalled = Cvar::Get("ramInstalled", "16777216");
        totalRamInstalled = ramInstalled->GetValue();

        Logger::Log("Initialising early-start components...");
        // early start components. eg the MMU. The MMU has to be available because
        // any memory mappings that get made need to be registered with it
        for (Component *component : components)
        {
            if (component->IsEarlyStart())
                component->Start();
        }

        Logger::Log("Initialising normal-start components...");
        // late start components
        for (Component *component : components)
        {
            if (!component->IsEarlyStart())
                component->Start();
        }
    }

    void Machine::Tick()
    {
        for (Component* component : components)
        {
            // 0 clockspeed = run AFAP
            // THis may not be a good idea. We may have to add fake cycles.
            if (component->GetClockSpeed() > 0)
            {
                // maybe microseconds would be better ???
                auto ns = Chrono_GetTicksNS(Chrono_GetTime());
                bool run = false;

                if (component->delayNs != 0)
                {
                    auto nsPerTick = (1.0 / (double)component->GetClockSpeed()) * 1000000000; // use maximum precision available

                    if (component->lastTickNs != 0 || (ns - component->lastTickNs) > nsPerTick)
                        run = true;
                }
                else // slower, delay timing
                {
                    if ((ns - component->lastTickNs) > component->delayNs)
                        run = true;
                }

                if (run)
                {
                    component->lastTickNs = ns;
                    goto run;
                }
            }
            else
                goto run;

        run:
            component->Tick();
        }
    }

    void Machine::OnEvent(Event &evt)
    {
        for (Component* component : components)
        {
            component->OnEvent(evt);
        }
    }

    void Machine::Render(RenderTexture* screen)
    {
        for (Component* component : components)
        {
            component->Render(screen);
        }
    }

    void Machine::Shutdown()
    {
        for (Component* component : components)
        {
            component->Shutdown();
            components.pop_back();
            delete component;
        }
    }
}