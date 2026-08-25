/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    ip2_dip_switches.hpp: Implements the system configuration DIP switches at 31800000 on the IP2 and a Coherent extension for them
*/

#pragma once
#include <coherent/coherent.hpp>

#define LOG_PREFIX_IP2SWITCHES              "IP2 Switches"

namespace Motion
{
    class CoherentExtensionIP2Switches : public CoherentExtension
    {
    public:
        CoherentExtensionIP2Switches(Component* owner) : CoherentExtension(owner) {}

        CoherentExtensionType GetExtensionType() override { return CoherentExtensionType::CustomMenu; }; 
        const char* GetMenuName() override { return "Backpanel Switches"; };

        void AddUI() override;
    };

    class IP2Switches : public Component
    {
        friend class CoherentExtensionIP2Switches;

        #define SWITCH_ADDR                     0x31800000

        // DIp switch purposes
        // _END is used to make some things easier in the debug ui

        #define SWITCH_IS_SLAVE                 (1 << 15)
        #define SWITCH_PRIMARY_DISPLAY_TYPE     (1 << 10 | 1 << 9 | 1 << 8) // primary display types
        #define SWITCH_PRIMARY_DISPLAY_TYPE_END 8                          // primary display types

        #define SWITCH_DISP_PROGRESSIVE         0x0                         // Progressive-scan 60hz monitor
        #define SWITCH_DISP_INTERLACED          0x1                         // Interlaced 30hz monitor
        #define SWITCH_DISP_TV_NTSC             0x2                         // NTSC Television
        #define SWITCH_DISP_TV_PAL              0x3                         // PAL Television ("bad" according to SGI)

        #define SWITCH_RS232_SPEED              (1 << 12 | 1 << 11)         // rs232 baud speed 
        #define SWITCH_RS232_SPEED_END          11         

        #define SWITCH_RS232_SPEED_9600         0x0                         // run port2 at 9600 baud
        #define SWITCH_RS232_SPEED_300          0x1                         // run port2 at 300 baud
        #define SWITCH_RS232_SPEED_1200         0x2                         // run port2 at 1200 baud
        #define SWITCH_RS232_SPEED_19200        0x3                         // run port2 at 19200 baud
        #define SWITCH_RS232_SPEED_600          0x4                         // you acnnot set this because it is only 2 bits lol

        #define SWITCH_USE_SECONDARY_DISP       (1 << 6)                    // use secondary display
        #define SWITCH_SHUTUP_PROM              (1 << 5)                    // shut up infinite prom loging
        #define SWITCH_AUTOBOOT                 (1 << 4)                    // autoboot or boot to prom
        #define SWITCH_BOOT_TYPE                (1 << 3 | 1 << 2 | 1 << 1 | 1 << 0)  // boot device
        #define SWITCH_BOOT_TYPE_END            0  

        #define SWITCH_BOOT_DEFAULT_HDD        0x0                          // boot from default hdd
        #define SWITCH_BOOT_DEFAULT_TAPE       0x1                          // boot from default tape
        #define SWITCH_BOOT_DEFAULT_FLOPPY     0x2                          // boot from defualt 5.25" floppy
        #define SWITCH_BOOT_DEFAULT_XNS        0x3                          // Ethernet XNS Netboot
        #define SWITCH_BOOT_PROM_MONITOR       0x5                          // boot to prom monitor
        #define SWITCH_BOOT_EPROM_BOARD        0x6                          // boot from an eprom board
        #define SWITCH_BOOT_DEVICE_IP          0x9                          // boot from Interphase SMD disk 
        #define SWITCH_BOOT_DEVICE_ST          0xA                          // boot from storager tape [stX]
        #define SWITCH_BOOT_DEVICE_SF          0xB                          // boot from storager floppy [sfX]
        #define SWITCH_BOOT_DEVICE_SD          0xC                          // boot from storager ESDI HDD [sdX]
        #define SWITCH_BOOT_DEVICE_MT          0xD                          // boot from DSD tape [mtX]
        #define SWITCH_BOOT_DEVICE_MF          0xE                          // boot from DSD floppy [mfX]
        #define SWITCH_BOOT_DEVICE_MD          0xF                          // boot from DSD HDD [mdX]

        // 0x0D ... 0x0F force DSD boot but idk why the user should cfg them

        public:
            void Start() override;

            uint8_t Read8(size_t addr) override;
            uint16_t Read16(size_t addr) override;
            uint32_t Read32(size_t addr) override;
            void Write8(size_t addr, uint8_t value) override;
            void Write16(size_t addr, uint16_t value) override;
            void Write32(size_t addr, uint32_t value) override; 

            const char* GetName() { return "IP2 Back Panel Switches"; };
        private: 
            uint16_t switchState; 
            AddrSpaceMapping mapping;
            CoherentExtensionIP2Switches* switchExtension;
    };
}
