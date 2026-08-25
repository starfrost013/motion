#include <component/ip2/ip2_dip_switches.hpp>   

namespace Motion
{
    Cvar* bootDevice;

    /*
        Which device the PROM loads the kernel from. The PROM's own help text names them:

            "sd(or si) - storager disk."   "st(or sq) - storager cartridge tape."
            "sf        - storager floppy disk."

        so a machine whose only disk hangs off a Storager wants sd here. md, the DSD winchester, is
        the default because that is the controller the shipped disk image is labelled for.
    */
    struct BootDeviceName
    {
        const char* name;
        uint16_t value;
    };

    static const BootDeviceName bootDeviceNames[] =
    {
        { "hdd",     SWITCH_BOOT_DEFAULT_HDD },      // whatever the PROM considers the default
        { "tape",    SWITCH_BOOT_DEFAULT_TAPE },
        { "floppy",  SWITCH_BOOT_DEFAULT_FLOPPY },
        { "xns",     SWITCH_BOOT_DEFAULT_XNS },
        { "prom",    SWITCH_BOOT_PROM_MONITOR },
        { "eprom",   SWITCH_BOOT_EPROM_BOARD },
        { "ip",      SWITCH_BOOT_DEVICE_IP },        // Interphase 2190 SMD disk
        { "st",      SWITCH_BOOT_DEVICE_ST },        // Storager cartridge tape
        { "sf",      SWITCH_BOOT_DEVICE_SF },        // Storager floppy
        { "sd",      SWITCH_BOOT_DEVICE_SD },        // Storager disk - si0 to the kernel
        { "mt",      SWITCH_BOOT_DEVICE_MT },        // DSD tape
        { "mf",      SWITCH_BOOT_DEVICE_MF },        // DSD floppy
        { "md",      SWITCH_BOOT_DEVICE_MD },        // DSD winchester
    };

    void IP2Switches::Start()
    {
        AddrSpaceMapping mapping = AddrSpaceMapping();

        mapping.component = this;
        mapping.startAddr = SWITCH_ADDR;
        mapping.endAddr = SWITCH_ADDR + 1;

        AddrSpace::AddMapping(mapping);

        switchExtension = new CoherentExtensionIP2Switches(this);
        Coherent::RegisterExtension(switchExtension);

        // setup reasonable defaults - this used to be left uninitialised, so which device the
        // PROM tried to boot from was down to whatever happened to be in the heap that day.
        switchState = (SWITCH_AUTOBOOT | SWITCH_BOOT_DEVICE_MD);

        bootDevice = Cvar::Get("bootDevice", "md");

        for (const BootDeviceName& entry : bootDeviceNames)
        {
            if (strcmp(bootDevice->GetString(), entry.name))
                continue;

            switchState = (switchState & ~SWITCH_BOOT_TYPE) | (entry.value << SWITCH_BOOT_TYPE_END);

            Logger::Log(LOG_PREFIX_IP2SWITCHES, std::format("Boot device switch set to {} (0x{:x})",
                entry.name, entry.value).c_str());
            return;
        }

        Logger::Log(LOG_PREFIX_IP2SWITCHES, std::format("+set bootDevice {} is not a device this PROM knows about - "
            "staying on md. Try one of hdd, tape, floppy, xns, prom, eprom, ip, st, sf, sd, mt, mf, md.",
            bootDevice->GetString()).c_str(), LogChannels::Warning);
    }

    //
    // STATUS REG 
    //

    uint8_t IP2Switches::Read8(size_t addr)
    { 
        // only two addresses, lazy mode
        // BIG ENDIAN !!!! 
        if (addr & 0x01)
            return (switchState >> 8) & 0xFF;
        else
            return (switchState & 0xFF00);
    };

    uint16_t IP2Switches::Read16(size_t addr) { return switchState; };

    // not sure how this behaves on real h/w, if its open bus, 0 or sign extended etc
    uint32_t IP2Switches::Read32(size_t addr) { return switchState; };
    
    void IP2Switches::Write8(size_t addr, uint8_t value)
    {
        // BIG ENDIAN !!!! 
        if (addr & 0x01)
            switchState = (value & 0xFF00) | value;
        else
            switchState = (value << 8) | (value & 0xFF);
    };

    void IP2Switches::Write16(size_t addr, uint16_t value) { switchState = value; };

    // not sure how this behaves on real h/w, if its open bus, 0 or sign extended etc
    void IP2Switches::Write32(size_t addr, uint32_t value) { switchState = value; }; 

    //
    // COHERENT debugger extension
    //

    void CoherentExtensionIP2Switches::AddUI()
    {
        // cond for determing checkbox state of menu
        bool cond = false;
        IP2Switches* switches = (IP2Switches*)component;

        // bits 15:14
        cond = (switches->switchState & SWITCH_IS_SLAVE);

        if (ImGui::MenuItem("This IRIS is Slave (switch multibus area)", nullptr, &cond))
            FLIP_MASK_STATE(switches->switchState, SWITCH_IS_SLAVE);

        if (ImGui::BeginMenu("Display Type"))
        {
            cond = (switches->switchState & SWITCH_PRIMARY_DISPLAY_TYPE) == (SWITCH_DISP_PROGRESSIVE << SWITCH_PRIMARY_DISPLAY_TYPE_END);

            if (ImGui::MenuItem("Non-interlaced 60Hz Monitor", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_PRIMARY_DISPLAY_TYPE);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_DISP_PROGRESSIVE << SWITCH_PRIMARY_DISPLAY_TYPE_END));
            }

            cond = (switches->switchState & SWITCH_PRIMARY_DISPLAY_TYPE) == (SWITCH_DISP_INTERLACED << SWITCH_PRIMARY_DISPLAY_TYPE_END);

            if (ImGui::MenuItem("Interlaced 30Hz monitor", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_PRIMARY_DISPLAY_TYPE);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_DISP_INTERLACED << SWITCH_PRIMARY_DISPLAY_TYPE_END));

            }

            cond = (switches->switchState & SWITCH_PRIMARY_DISPLAY_TYPE) == (SWITCH_DISP_TV_NTSC << SWITCH_PRIMARY_DISPLAY_TYPE_END);

            if (ImGui::MenuItem("NTSC Television", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_PRIMARY_DISPLAY_TYPE);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_DISP_TV_NTSC << SWITCH_PRIMARY_DISPLAY_TYPE_END));
            }

            cond = (switches->switchState & SWITCH_PRIMARY_DISPLAY_TYPE) == (SWITCH_DISP_TV_PAL << SWITCH_PRIMARY_DISPLAY_TYPE_END);

            if (ImGui::MenuItem("PAL Television (\"bad\" - SGI)", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_PRIMARY_DISPLAY_TYPE);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_DISP_TV_PAL << SWITCH_PRIMARY_DISPLAY_TYPE_END));
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("RS232 Baud Rate"))
        {
            cond = (switches->switchState & SWITCH_RS232_SPEED) == (SWITCH_RS232_SPEED_9600 << SWITCH_RS232_SPEED_END);

            if (ImGui::MenuItem("9600", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_RS232_SPEED);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_RS232_SPEED_9600 << SWITCH_RS232_SPEED_END));
            }

            cond = (switches->switchState & SWITCH_RS232_SPEED) == (SWITCH_RS232_SPEED_300 << SWITCH_RS232_SPEED_END);

            if (ImGui::MenuItem("300", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_RS232_SPEED);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_RS232_SPEED_300 << SWITCH_RS232_SPEED_END));
            }

            cond = (switches->switchState & SWITCH_RS232_SPEED) == (SWITCH_RS232_SPEED_1200 << SWITCH_RS232_SPEED_END);

            if (ImGui::MenuItem("1200", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_RS232_SPEED);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_RS232_SPEED_1200 << SWITCH_RS232_SPEED_END));
            }

            cond = (switches->switchState & SWITCH_RS232_SPEED) == (SWITCH_RS232_SPEED_19200 << SWITCH_RS232_SPEED_END);

            if (ImGui::MenuItem("19200", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_RS232_SPEED);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_RS232_SPEED_19200 << SWITCH_RS232_SPEED_END));
            }

            // there is 600 baud but it requires this to be 0x04 which it cannot be. maybe it works if you overflow....
            ImGui::EndMenu();
        }

        cond = (switches->switchState & SWITCH_USE_SECONDARY_DISP);

        if (ImGui::MenuItem("Boot using Secondary Display", nullptr, &cond))
            FLIP_MASK_STATE(switches->switchState, SWITCH_USE_SECONDARY_DISP);

        cond = (switches->switchState & SWITCH_SHUTUP_PROM);

        if (ImGui::MenuItem("Shut up PROM", nullptr, &cond))
            FLIP_MASK_STATE(switches->switchState, SWITCH_SHUTUP_PROM);

        cond = (switches->switchState & SWITCH_AUTOBOOT);

        if (ImGui::MenuItem("Autoboot", nullptr, &cond))
            FLIP_MASK_STATE(switches->switchState, SWITCH_AUTOBOOT);

        if (ImGui::BeginMenu("Boot Type"))
        {
            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_DEFAULT_HDD;

            if (ImGui::MenuItem("HDD [try ipX, stX, then mdX]", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_TYPE);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_DEFAULT_HDD << SWITCH_BOOT_TYPE_END));
            }

            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_DEFAULT_TAPE;

            if (ImGui::MenuItem("Cartridge Tape", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_TYPE);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_DEFAULT_TAPE << SWITCH_BOOT_TYPE_END));
            }

            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_DEFAULT_FLOPPY;

            if (ImGui::MenuItem("Floppy [tries sfX, then mdX]", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_TYPE);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_DEFAULT_FLOPPY << SWITCH_BOOT_TYPE_END));
            }

            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_DEFAULT_XNS;

            if (ImGui::MenuItem("XNS Ethernet Netboot", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_TYPE);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_DEFAULT_XNS << SWITCH_BOOT_TYPE_END));
            }
     
            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_PROM_MONITOR;

            if (ImGui::MenuItem("Boot to PROM Monitor", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_TYPE);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_PROM_MONITOR << SWITCH_BOOT_TYPE_END));
            }
    
            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_EPROM_BOARD;

            if (ImGui::MenuItem("Boot from EPROM Board", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_EPROM_BOARD);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_PROM_MONITOR << SWITCH_BOOT_TYPE_END));
            }

            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_DEVICE_IP;

            if (ImGui::MenuItem("[Interphase SMD 2190] HDD [ipX]", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_DEVICE_IP);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_DEVICE_IP << SWITCH_BOOT_TYPE_END));
            }

            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_DEVICE_ST;

            if (ImGui::MenuItem("[Storager 3030] Tape [stX]", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_DEVICE_ST);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_DEVICE_ST << SWITCH_BOOT_TYPE_END));
            }


            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_DEVICE_SF;

            if (ImGui::MenuItem("[Storager 3030] Floppy [sfX]", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_DEVICE_SF);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_DEVICE_SF << SWITCH_BOOT_TYPE_END));
            }

            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_DEVICE_SD;

            if (ImGui::MenuItem("[Storager 3030] HDD [sdX]", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_DEVICE_SD);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_DEVICE_SD << SWITCH_BOOT_TYPE_END));
            }

            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_DEVICE_MT;

            if (ImGui::MenuItem("[DSD 2917] Tape [mtX]", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_DEVICE_MT);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_DEVICE_MT << SWITCH_BOOT_TYPE_END));
            }

            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_DEVICE_MF;

            if (ImGui::MenuItem("[DSD 2917] Tape [mfX]", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_DEVICE_MF);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_DEVICE_MF << SWITCH_BOOT_TYPE_END));
            }

            cond = (switches->switchState & SWITCH_BOOT_TYPE) == SWITCH_BOOT_DEVICE_MD;

            if (ImGui::MenuItem("[DSD 2917] HDD [mdX]", nullptr, &cond))
            {
                switches->switchState &= (~SWITCH_BOOT_DEVICE_MD);
                FLIP_MASK_STATE(switches->switchState, (SWITCH_BOOT_DEVICE_MD << SWITCH_BOOT_TYPE_END));
            }

            ImGui::EndMenu();
        }
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 0.0, 0.0, 1.0));
        ImGui::MenuItem("** Not all options will work **");
        ImGui::PopStyleColor();
    }
}