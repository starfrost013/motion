/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    gf2_coordinator.hpp: The mappings for GF2 are messy as shit. So we map them to different components in here...
*/

#pragma once
#include <component/component.hpp>
#include <component/multibus/multibus.hpp>
#include <component/gpu/vram.hpp>
#include <component/gpu/juniper/gf2/am2903/am2903.hpp>

namespace Motion
{    
    Cvar* disableGfx; 
    
    class GF2FBC
    {
    public: 
        void Start();
        void Tick();

        const char* GetName() { return "Framebuffer & Bitplane Controller (AMD Am2903)"; }; 
        
    private:
        AM2903 Am2903; 
    }; 

    // sometimes it is x50001000 for BETA GF2 ????? 
    // TOKEN means BUSY (SET == BUSY)

    #define GF2_MULTIBUS_START                      0x50002000
    // A10 LINE select s the registers
    #define GF2_FBC_FLAGS                           0x50002400 

    #define GF2_FBC_FLAGS_READ_GE_REQ_TO_FBC        (1 << 0)
    #define GF2_FBC_FLAGS_READ_FBC_ACK_GE           (1 << 1)
    #define GF2_FBC_FLAGS_READ_GEPA_TRIP_IN         (1 << 2)
    #define GF2_FBC_FLAGS_READ_GEPA_TRAP_OUT        (1 << 3)
    #define GF2_FBC_FLAGS_READ_FBC_INTERRUPT        (1 << 4)
    #define GF2_FBC_FLAGS_READ_FBC_GE_PORT_TOKEN    (1 << 5)
    #define GF2_FBC_FLAGS_READ_FBC_BPC_VERTICAL_INT (1 << 6)
    #define GF2_FBC_FLAGS_READ_FBC_NEW_VERTICAL_INT (1 << 7)
    #define GF2_FBC_FLAGS_READ_FBC_NEEDS_INPUT      (1 << 8)
    #define GF2_FBC_FLAGS_READ_FBC_ACK              (1 << 10)
    #define GF2_FBC_FLAGS_READ_BPC_ACK              (1 << 11)

    #define GF2_FBC_FLAGS_WRITE_RUN                 (1 << 0)    // also MAINTSEL0
    #define GF2_FBC_FLAGS_WRITE_SUBSTI              (1 << 1)    // also MAINTSEL1
    #define GF2_FBC_FLAGS_WRITE_HOSTFLAG            (1 << 2)
    #define GF2_FBC_FLAGS_WRITE_MAINT               (1 << 3)
    #define GF2_FBC_FLAGS_WRITE_FORCE_REQUEST       (1 << 4)           
    #define GF2_FBC_FLAGS_WRITE_FORCE_ACKNOWLEDGE   (1 << 5)           
    #define GF2_FBC_FLAGS_WRITE_FORCE_SUBST_IN      (1 << 6)           
    #define GF2_FBC_FLAGS_WRITE_FORCE_SUBST_OUT     (1 << 7)                    

    #define GF2_FBC_DATA                            0x50002800  // A VERY IMPORTANT REGISTER
    #define GF2_GE_FLAGS                            0x50002C00

    #define GF2_GE_FLAG_RESET                       (1 << 0)    // is the GE reset?
    #define GF2_GE_FLAG_SUBST_BPC_CODE              (1 << 1)    // replace BPC command bits [0.3] with whatever is the di bus
    #define GF2_GE_FLAG_ENABLE_FIFO_INT             (1 << 2)    // enable fifo int
    #define GF2_GE_FLAG_ENABLE_VERT_INT             (1 << 3)    // enable vert int
    #define GF2_GE_FLAG_ENABLE_FBC_INT              (1 << 10)   // enable fbc program (microcode) int
    #define GF2_GE_FLAG_ENABLE_AUTOCLEAR            (1 << 11)   // AUTO CLEAR fbc interrupts after writing
    #define GF2_GE_FLAG_ENABLE_UCODE_ACCESS         (1 << 15)   // Microcode access enabled

    #define GF2_MULTIBUS_END                        0x50002FFF

    #define GF2_PRIVATE_BUS_START                   0x60000000
    #define GF2_GE_TOKEN                            0x60000000
    #define GF2_GE_DATA                             0x60001000  // THE MOST IMPORTANT REGISTER, THERE IS NO REGISTER MORE IMPORTANT THAN THIS ONE! EXECUTE ALL COMMANDS VIA HERE!
    #define GF2_PRIVATE_BUS_END                     0x60001FFF

    #define GF2_MULTIBUS_SLOT                       18

    #define GF2_GE_LOG_PREFIX                       "GF2 - Geometry Engine"

    // commands
    // GE commands onyl: we don't care about the FBC commands,
    // because we LLE the AM2903

    #define GE_CMD_WAITING		                    -1 

    #define GE_CMD_POPMM			                0x00
    #define GE_CMD_LOADMM		                    0x01
    #define GE_CMD_STOREMM		                    0x03
    #define GE_CMD_PUSHMM		                    0x04
    #define GE_CMD_LOADVIEWPORT		                0x05
    #define GE_CMD_SETHITMODE		                0x06
    #define GE_CMD_CLEARHITMODE		                0x07
    #define GE_CMD_PASSTHRU		                    0x08
    #define GE_CMD_PUSHVIEWPORT		                0x09
    #define GE_CMD_POPVIEWPORT		                0x0A
    #define GE_CMD_STOREVIEWPORT		            0x0B
    #define GE_CMD_RECONFIGURE		                0x0C
    #define GE_CMD_SWITCHPIPES		                0x0D
    #define GE_CMD_NOOP			                    0x0F
    #define GE_CMD_MOVE			                    0x10
    #define GE_CMD_DRAW			                    0x11
    #define GE_CMD_POINT			                0x12
    #define GE_CMD_CURVE			                0x13
    #define GE_CMD_MOVEREL		                    0x14
    #define GE_CMD_DRAWREL		                    0x15
    #define GE_CMD_POINTREL		                    0x16
    #define GE_CMD_MIDMM0		                    0x20
    #define GE_CMD_MIDMM1		                    0x21
    #define GE_CMD_MIDMM2		                    0x22
    #define GE_CMD_MIDMM3		                    0x23
    #define GE_CMD_FIRSTMM0		                    0x24
    #define GE_CMD_FIRSTMM1		                    0x25
    #define GE_CMD_FIRSTMM2		                    0x26
    #define GE_CMD_FIRSTMM3		                    0x27
    #define GE_CMD_LASTMM0		                    0x28
    #define GE_CMD_LASTMM1		                    0x29
    #define GE_CMD_LASTMM2		                    0x2A
    #define GE_CMD_LASTMM3		                    0x2B
    #define GE_CMD_COMPLETEMM0		                0x2C
    #define GE_CMD_COMPLETEMM1		                0x2D
    #define GE_CMD_COMPLETEMM2		                0x2E
    #define GE_CMD_COMPLETEMM3		                0x2F
    #define GE_CMD_MOVEPOLY		                    0x30
    #define GE_CMD_DRAWPOLY		                    0x31
    #define GE_CMD_CLOSEPOLY		                0x33
    #define GE_CMD_MOVEPOLYREL		                0x34
    #define GE_CMD_DRAWPOLYREL		                0x35
    #define GE_CMD_CURVEPOLY		                0x37
    #define GE_CMD_TRANSFORMPOINT	                0x38

    class GF2GE
    {
        friend class GF2Coordinator;
        
    public: 
        void Start();
        void Tick();
    private:
        GF2FBC* fbc; // needed for passthrough
        bool busy;  // token is passing through
    }; 

    class GF2Coordinator : public Component
    {
    public: 
        void Start() override; 
            
        uint8_t Read8(size_t addr) override;
        uint16_t Read16(size_t addr) override;
        uint32_t Read32(size_t addr) override;
        void Write8(size_t addr, uint8_t value) override;
        void Write16(size_t addr, uint16_t value) override;
        void Write32(size_t addr, uint32_t value) override; 

        void Tick() override;

        const char* GetName() { return "GF2 Board Coordinator (GE+FBC)"; }; 
    private: 
        Multibus* multibus;
        GF2GE ge;
        GF2FBC fbc;
    }; 
}; 