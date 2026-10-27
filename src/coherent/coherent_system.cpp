#include <coherent/coherent.hpp>

namespace Motion
{
    /// setters for private fields

    void CoherentSystem::SetRunState(CoherentSystem::RunState runState)
    {
        this->runState = runState;

        switch (runState)
        {
            case CoherentSystem::RunState::Reset:
                Emulation::Reset();
                break;
            case CoherentSystem::RunState::SingleStepNormal:
                Emulation::SingleStep();
                SetRunState(Paused);
                break;
            case CoherentSystem::RunState::SingleStepOver:
                Emulation::SingleStep();
                SetRunState(Paused);
                break;
            case CoherentSystem::RunState::Paused:
            case CoherentSystem::RunState::NotYetStarted:
                if (!Emulation::GetPaused())
                    Emulation::SetPaused(true);

                break;
            case CoherentSystem::RunState::Running:
                if (Emulation::GetPaused())
                    Emulation::SetPaused(false);
        }
    }


}