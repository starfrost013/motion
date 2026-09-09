/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    gf2_fbc.hpp: The Frame Buffer Controller, four AMD AM2903 chips running custom SGI Microcode.
*/

#pragma once
#include <component/component.hpp>
#include <component/gpu/juniper/gf2/gf2_fbc.hpp>

namespace Motion
{
    // sometimes it is 0x50001000 for BETA GF2 ????? 

    #define GF2_MULTIBUS_START                  0x50002000
    // A10 LINE select s the registers
    #define GF2_FBC_FLAGS                       0x50002400 

    #define GF2_FBC_FLAGS_GE_REQ_TO_FBC         (1 << 0)
    #define GF2_FBC_FLAGS_FBC_ACK_GE            (1 << 1)
    #define GF2_FBC_FLAGS_GEPA_TRIP_IN          (1 << 2)
    #define GF2_FBC_FLAGS_GEPA_TRAP_OUT         (1 << 3)
    #define GF2_FBC_FLAGS_FBC_INTERRUPT         (1 << 4)
    #define GF2_FBC_FLAGS_FBC_GE_PORT_TOKEN     (1 << 5)
    #define GF2_FBC_FLAGS_FBC_BPC_VERTICAL_INT  (1 << 6)
    #define GF2_FBC_FLAGS_FBC_NEW_VERTICAL_INT  (1 << 7)
    #define GF2_FBC_FLAGS_FBC_NEEDS_INPUT       (1 << 8)
    #define GF2_FBC_FLAGS_FBC_ACK               (1 << 10)
    #define GF2_FBC_FLAGS_BPC_ACK               (1 << 11)

    #define GF2_FBC_DATA                        0x50002800  // A VERY IMPORTANT REGISTER
    #define GF2_GE_FLAGS                        0x50002C00

    #define GF2_GE_FLAG_RESET                   (1 << 0)    // is the GE reset?
    #define GF2_GE_FLAG_SUBST_BPC_CODE          (1 << 1)    // replace BPC command bits [0.3] with whatever is the di bus
    #define GF2_GE_FLAG_ENABLE_FIFO_INT         (1 << 2)    // enable fifo int
    #define GF2_GE_FLAG_ENABLE_VERT_INT         (1 << 3)    // enable vert int
    #define GF2_GE_FLAG_ENABLE_FBC_INT          (1 << 10)   // enable fbc program (microcode) int
    #define GF2_GE_FLAG_ENABLE_AUTOCLEAR        (1 << 11)   // AUTO CLEAR fbc interrupts after writing
    #define GF2_GE_FLAG_ENABLE_UCODE_ACCESS     (1 << 15)   // Microcode access enabled

    #define GF2_MULTIBUS_END                    0x50002FFF

    #define GF2_PRIVATE_BUS_START               0x60000000
    #define GF2_GE_TOKEN                        0x60000000
    #define GF2_GE_DATA                         0x60001000  // THE MOST IMPORTANT REGISTER, THERE IS NO REGISTER MORE IMPORTANT THAN THIS ONE! EXECUTE ALL COMMANDS VIA HERE!
    #define GF2_PRIVATE_BUS_END                 0x60001FFF

    #define GF2_MULTIBUS_SLOT                   18

    #define GF2_GE_LOG_PREFIX                   "GF2 - Geometry Engine"

    class GF2GE
    {
    public: 
        const char* GetName() { return "Geometry Engine Rev 2.0/2.5"; }; 

        void Start();
        void Tick();
    private:
        GF2FBC* fbc; // needed for passthrough

    }; 
}; 