
/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    vram.hpp: Base class vram components
    Mostly just so we can search for vram components.
    ALL vram components are early start.
*/

#include <component/component.hpp>
#include <component/gpu/vram.hpp>

namespace Motion
{
    void ComponentVRAM::Start()
    {
        size_t vramSize = GetInternalFbSizeX() * GetInternalFbSizeY() * GetBytesPerPixel();
        // Zeroed, not just allocated: drawing keeps the planes the write mask leaves clear, so allocator junk shows through.
        vram = new uint8_t[vramSize]();

        Logger::Log(std::format("Video RAM size is {} bytes", vramSize).c_str());

        CoherentEditor::Settings settings;
        settings.buf = vram;
        settings.bufSize = GetCapacity();
        settings.name = "VRAM Editor";

        CoherentEditor* editor = new CoherentEditor(this, settings);
        Coherent::RegisterExtension(editor);
        
    }


    void ComponentVRAM::Shutdown()
    {
        delete[] vram;
    }
}