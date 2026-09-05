#include <iostream>
#include <component/cpu/mc68020.hpp>

namespace Motion
{
    void MC68020::Start()
    {
        // ensure we are in reset so e.g. the MMUs don't try and map everything
        isInReset = true;

        Logger::Log(LOG_PREFIX_68020, "*yawn* I'm a Motorola 68020!", LogChannels::Debug);

        moiraCpu.setModel(Motion::Lisburn::Model::M68020);

        system = new MC68020DebuggerSystem(&moiraCpu);
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[0], "d0"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[1], "d1"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[2], "d2"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[3], "d3"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[4], "d4"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[5], "d5"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[6], "d6"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.d[7], "d7"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[0], "a0"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[1], "a1"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[2], "a2"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[3], "a3"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[4], "a4"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[5], "a5"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[6], "a6"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.a[7], "sp [a7]"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.pc, "pc"));
        // NEED TO BE some changes to LISBURN for these ones
        //system->AddRegister(new CoherentSystem::Register<uint8_t>(&this->moiraCpu.reg), "ccr"); // flags (technically lower byte of status)
        //system->AddRegister(new CoherentSystem::Register<uint8_t>(&this->moiraCpu.reg.sr.), "status"); 
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.usp, "usp"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.msp, "msp"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.isp, "isp"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.sfc, "sfc"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.dfc, "dfc"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.vbr, "vbr"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.cacr, "cacr"));
        system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.caar, "caar"));
        //system->AddRegister(new CoherentSystem::Register<uint32_t>(&this->moiraCpu.reg.sr), "sr");

        // set the word size
        system->SetWordSize(CoherentSystem::WordSize::WordSize32);

        Coherent::SetSystem(system);
        // convert to nanoseconds

    }

    void MC68020::Reset()
    {
        AddrSpace::SetFaultsEnabled(false);

        Logger::Log(LOG_PREFIX_68020, "Resetting CPU...");

        // ignore autovectors. THe IP2 reads out of PROM U118 to determine what vectors to call  on interrupts
        moiraCpu.irqMode = Motion::Lisburn::IrqMode::USER;
        moiraCpu.reset();

        // moira has a didReset delegate, but due to various design reasons (mostly include cycles) we can't use it 
        isInReset = false;

        // so we don't bus fault 8 morbillion times during reset
        AddrSpace::SetFaultsEnabled(true);
    }

    void MC68020::Tick()
    {
        moiraCpu.execute();
    }
    
    void MC68020::Shutdown()
    {
        delete system;
        Coherent::SetSystem(nullptr);
    }
}