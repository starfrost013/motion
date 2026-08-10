
/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    vram.hpp: Base class vram components
    Mostly just so we can search for vram components.
    ALL vram components are early start.
*/

#pragma once
#include <component/component.hpp>
#include <coherent/coherent_editor.hpp>

namespace Motion
{
    class ComponentVRAM : public Component
    {
    public:
        bool IsEarlyStart() override { return true; }; 
        void Start() override; 
        void Shutdown() override;

        // returs
        virtual int32_t GetInternalFbSizeX() { return 0; };
        virtual int32_t GetInternalFbSizeY() { return 0; };

        // We model the VRAM as a 1024*1024*4 type tihng.
        virtual int32_t GetBytesPerPixel() { return 0; }; 

        /// @brief get the vram address for a coordinate
        /// @param x the x coordinate
        /// @param y the y coordinate
        /// @return the vram address that maps to that coordinate.
        virtual size_t GetVramAddress(int32_t x, int32_t y) { return 0; };

        // Getters for private
        uint8_t* GetPixels() { return vram; };
        size_t GetCapacity() { return GetInternalFbSizeX() * GetInternalFbSizeY() * GetBytesPerPixel();  };

        // Setters for private
    protected: 
        uint8_t* vram; 
        CoherentEditor* vramEditor;
    };
};