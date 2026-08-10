M  O  T  I  O  N
The SGI Emulator 

Version 0.1.0
Copyright (C) 2026 starfrost

Currently this emulator targets the 68020-based IRIS 3000 series machines only.

Here's a basic guide for using the emulator:

ROMs go in the ROMs folder

THE MAIN WINDOW:

Emulator Log window - shows you the log of what is happening
Coherent window - debugger
    TOP BAR:
        Pause CPU - Pause the CPU
        Reset - Reset emulation 
        Step - single step (only if paused)
        
    LEFT BAR:
        Shows register view of the 68020 CPU.

    MIDDLE BAR:
        Shows the next 30 instructions of the 68020 CPU; the currently executed instruction is highlighted in blue.

    RIGHT BAR:
        Breakpoints
            Input an address into the text box and press Add to add a breakpoint.
            Click on a breakpoint to select it (due to IMGUI weirdness, currently it doesn't show a colour while selected.)
            If you have any breakpoints selected clicking remove will remove them.
        Watchpoints
            Will show you the 32-bit value of any memory address you put in.
        Stack
            Shows the top 8 dwords in the stack. A bit useless, since variables and parameters and such are oftne pushed onto the stack.
            Eventually I'll profile calls and such to create a real call stack.

    MENU ITEMS:

        Peripherals - lets you access the peripheral debuggers.
            IP2 MMU - Debug SGI's TTL MMU and view the pagetable.
            DUART - Debug the serial DUARTs.
            DC4 - Debug the GPU's display contrroller (DC4) board

    Style - lets you change style. The styles currently suck
    Backpanel Switches - reconfigure the IRIS's back panel switches.
    Serial Console - Access the PROM console if enabled and view information about the state of the UARTs, ? is help.

Command line:
+set - set a convar. Follow with a value, "1" to enable "0" to disable.
    Logging: (warnings and errors are always printed)
        logIP2MMU 
            Log the IP2 MMU (a custom TTL job by SGI).
        logIP2DUART 
            Enables debug log messages for the IP2 DUART (two Signetics SCN68681 DUARTs).
        logIP2RTC
            Enables debug log messages for the IP2 RTC, which is currently a stub emulation. 
        logDC4
            Enables debug log messages for the Display Controller.
        logUC4
            Enables debug log messages for the Update Controller.
        logChannels
            Provide a custom log channel mask. Default is -1, which means "use the emulator's default settings":

            Masks:
                0x00000001 - Allow Message level logs to be displayed.
                0x00000002 - Allow Warning level logs to be displayed.
                0x00000004 - Allow Error level logs to be displayed.
                0x00000008 - Allow FatalError level logs to be displayed.
                0x00000010 - Allow UnsafeShutdown level logs to be displayed.

        logDestinations
            Provide a custom log destination mask. Default is -1, which means "use the emulator's default settings".

            Masks:
                0x00000001 - Log all messages to stdout.
                0x00000002 - Log all messages to stderr.
                0x00000004 - Log all messages to file.
    Configuration: 

        profileFolder - the location of the profile
            RESET the profile - delete every folder in this folder.

            Profile files:
                ip2_sram.bin: Private PROM SRAM.
        promPath
            the path of the PROM (basically the BIOS) to load.
            If you are messing around with different versions of the BIOS oyu can set this
        forceEnterSerialMonitor
            Disconnects the keyboard from the emulated machine. This forces the PROM to enter serial communication mode over DUART0 Port B.
            Since the graphics system (and multibus) don't work yet this is the only way to do anything with the machine right now.
        startPaused (default is 1)
            Start the emulator paused, 0 will start the CPU immediately.
        ramInstalled (default is 16777216)
            RAM installed in bytes.
            Must be between 0 (in which case only the serial PROM console will be available) and 33534432 / 32MB (anything above 16MB was never available as an 
            official config, but seems to work).
        numBitplanes (default is 32)
            The number of bitplanes (one BP3 board is 4 bitplanes) to install. Affects maximum graphics bit depth and available graphics modes.
            Must be a multiple of 4 and between 4 and 32. There is no real reason to change this since oyu can use mapped modes with more bitplanes.

Notes:
    "Unmapped write" or "Unmapped read" warnings: Ignore them, they are fine.

    If the PROM spits out a screen of the type:

        Fault Information (vector offset: xxxx):
        Exception: xxxxxxxxxxxxxxxxxx (Vector #xx)

        Processor Registers (ssp: xxxxxxxx):
            pc: xxxxxxxx  sr: xxxx

        Board Registers:
            text (base/limit): xxxx/xxxx
            stk  (base/limit): xxxx/xxxx
            status: xxxx parctl: xx mbp: xx

    Then it crashed. Unless you were using the edit memory commands this should never happen.
    If you did not intend to crash the machine and weren't trying to edit memory, take a screenshot of the screen and send it to me. 

    UNIX panic advice: 
        You can't run unix on here yet.
        
Not done:
    - Reconfigurable machines
    - Debugger commands
    - Configuration
    - Most things emulation wise