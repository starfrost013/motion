# Motion Changelog

Similar to a .plan file

## 2026-07-28

* added kernel and stack segment memory mappings
    * will be tied into mmu MemorySegment system
* release v0.0728 

## 2026-07-29

* implemented basic mmu address translation
    * ComponentMMU, has a translate method that returns a bool if a bus error occurred and takes a uint32_t* with a pointer to a final address to fill in.
        * I could just return a tuple. But this is critical path stuff.
        * Enough for the PROM to detect that we have 16 MB, rather than 0 or 1 MB.
        * Likely required also to run UNIX.
        * Mame was wrong - 13 bits instead of 10. Not sure why it works? Maybe I am wrong or maybe he only tested with limited ram
        * also has a start and shutdown method to measure tearing down and setting up the mmu in the addrspace system.
* guy wanted to add github actions support
    * he only did macos and linux. no windows. i can add windows later.
* added pagetable debug window
    * it sucks

## 2026-07-30

* this emulator hasn't been called iris in forever and now people know about it so change the namespace to motion and rename Iris.hpp to Motion.hpp.
* get rid of catchpoints due to UI space. instead we will have an option that allows to break on exception and to allow you to specify a simple ID to break on
* incremented coherent version to v0.6 (v0.5 = v0.0728 verison)
    * refactored the totally awful coherent file structure
        * coherentsystem now in coherent_system.cpp
    * added stack window and options menu
* made the event system work
    * don't like it since it calls empty methods. but having io device specific interface seemed too brittle.
* keyboard now correctly configures itself
    * however this makes the PROM not work.

## 2026-07-31

* nothing really

## 2026-08-01

* implemented key and mouse events after figuring out the keyboard only translates keys. doh!
    * need to translate ASCII to SGI
* translating SDL keycodes to SGI and sending them along on key is enough to get to a loop. it seems like it is trying to do a lot of i/o so maybe it is in graphics mode
    * PROM explodes and enters serial mode if you press a key too quickly during boot, so just odn't allow keypresses until keyboard init
    * typing PROM commands proves that its the PROM console, and that it works. cool!
* started multibus

## 2026-08-02

* wrote the first pass of the rendering api
    * everything is a render pass
    * backend-independent textures, inheriting from rendertexture 
        * sdl gpu textures are pushed to the gpu at the end of each frame. this should be in its own thread like iris
    *  since a uint8_t* of pixels is renderer independent and we will just be pushing to the gpu, make rendertexture nonvirtual for now. We can always change it later (woo, oop). it would likely be very slow to run a check all the time, so implement asserts for hotpath sanity checks (i.e. should never happen on a release build of the emulator.)
    * Implemented a basic renderer that has a single gpu copy pass that uploads a screen image to the screen as modified by any number of previous render passes. It shares  command buffer and swapchain texture with imgui

## 2026-08-03

* wrote multibus io methods
    * 20 slots, basic c++ array. slots are added by components and multibus stores a pointer to tehm.
    * we cache which slot is being accessed to prevent having to iterate through them.
        * TODO: Actually move this into the main emulation.
    * has a basic coherent extension
* implemented early-start properly so we don't do the ugly crock of checking if a component exists in tick and then getitng it.
* CPUMC68020::IsPrivilegedMode is a real thing now.
* made Coherent eextensions control their own type and menu name
    * replaced setextensiontype and setmenuname with getextensiontype and etmenuname that return hardcoded strings
* Make placeholder screen only drawn without anything else being drawn
* DC4/UC4 skeleton
* reconfigurable ram sizes

## 2026-08-04

* DC4 colourmap emulation
    * low 4 bits of flags set mapped colourmap, 0 = use 4096 colour map
* CoherentUI::COLOUR_HEADER added, temporary until we have a proper theme system (the current one sucks and needs to be rewritten)

## 2026-08-05 / 2026-08-08 

* VRAM addressing + BP3 board emumlation
    * basically linear. which bp3 planes exist apply a write mask, so if you have 16 bitplanes you write to memory basically anded with 0xffff
* allow drawing of non-screen-sized texture (introduce RenderTextureDrawMode)
* decouple window and renderer (window is now a renderer-backend-independent virtual Window class)
* Move code for adding components from emulation to machine and make it a virtual method so that we can have multiple machines; add the machineName convar.
* user overridable log destination / channel mask behaviour with logDestinations and logChannels cvar
* issue: currently renderer is dependent on some of the methods of machine
    * currently solved by initialising and starting the machine separately.
* OnRead/OnWrite* methods is now just Read*/Write*
* removed component determination truth value obtaining methods (ismmu, iscpu, isserialport) as they are useless
    * since we only really call the templated FindComponentByType methods once in start...
* UC4 command system
    * UC_WECODEAB, UC_WECODECD, UC_COLORCODEAB, UC_COLORCODECD UC_FILLRECT, UC_WRITEFONT, UC_DRAWCHAR all work.
* UC4 I/O system
    * All buffers

* After 11 hour marathon programming session (you can tell when a milesotne is apporacing)
    * PROM graphics

## 2026-08-09
* Bugfixing for v0.1 release:
    - Add UC4 mode / config debug.
    - Add Coherent::Exception and break on exception
    - Fix DC4 window scroll
    - Fix reset

## 2026-08-10
* v0.1.1 & v0.1.2
    * fix memory corruption bugs especially on shutdown (many new/delete vs new[]/delete[] mismatches)
    * fix emulator log window
    * create sram folder if it does not exist
    * add memory / vram viewer

## 2026-08-11
* add program state system for launcher
    * program state is now controlled by top level Program class
    * decoupled emulator state from program, renderer is now controlled by the program
    * removed renderer dependency on machine code. 
        * added SetScreenSize to allow the screen texture to be re-created (recreates the transfer buffer too) based on the fb size of the selected machine
    * split event system from render code so that, only the emulator can pump events.
        * Renderer::PumpEmulatorEventSystem is a temporary thing.
* Cvar::GetName was missing for some reason ?!
* flexion contributed video scaling (thanks)
    * and --help!

## 2026-08-13
* add actual menu items to the launcher
* lisburn: hardcode DASM and regsiter info to ON since we really don't need them to be an option
* DSD5217 headers
* Segment 4 (Multibus Memory) can be mapped
* Fix emulator input being sent to main emulator as well as IMGUI
* Fix shift and ctrl keys
* Fix busted back switches impl (was using pm2/???)

## 2026-08-14
* Added last megabyte multibus mapping (memory by default unless MMIO mapped)
* Added ascii view to memory viewer

## 2026-08-15
* Refactor multibus to support multiple mappings per slot and update debug window to do so.

## 2026-08-16
* did nothing other than half assed non working step over impl
* merge the code from flexion to fix my brain dead ctrl handling

## 2026-08-17
* figured out pointers to DSD structs. just convert them to offsets and go
    * enough to get past minit...stuck in mstatus.
* add coherent DSD stuff
* fix mapping of DSD. dynamically configure mapping based on WakeUp Board address 
* add messages for unimplemented read/write methods
* allow 8 bit reads from UCR register of UC4 as they are done  
* change description of bp3 from "BP3 Bitplaned VRAM" to "Video RAM (BP3 bitplane board)" to make it easier to understand 


## 2026-08-19
* remove some parts of sdl 
* added cool logo. consdiered sdl_image but didn't use it
* fix endianness of 16 bit writes to DSD 5217

## 2026-08-20
* got mad yelling at dsd buffers 
* figured out iopb->dba has multiple meanings so abstract it behind readbuffer/writebuffer and databuffertype
* merge inib and fmtb into inist

## 2026-08-21 & 2026-08-22
* many fixes of DSD 5217, but not enough for it to actually boot the kernel.
* let's do something else
* register editing!
    * updated :;write to actually dereference
    * converrt to uint64_t and then t so ti works for all numeric types
* multibus paging  
    * 256 registers at 44100000...441ff000 with page numbers for MULTIBUS
* still kludgy last 1meg mapping because bus model is abit fucked
    * merge IP2MMU With multibus?
* enough to get over very bad copy bug

## 2026-08-23 to 2026-08-27
* to keep a long story short, rewrote a bunch of originally-AI code: DSD 5217 emulation based on danifunker's
* IP2 U118 vectors & Multibus implementation
* yea it boots (panic: something wrong with intpixel32)
* RTC emulation

## 2026-08-27
* unaligned ram accesses
* fixed init order issue causing first instruction to be corrupted
* SRAM Editor
* can partially boot unix 

## 2026-08-29
* added AddrSpace::PeekXX and isPeek boolean to MMU Translation to suppress logging
    * not razy about this
    * these simply translate and return if bus error, bus 
    * Moira::read16Dasm does this

## 2026-08-31
* added Moira delegate, didReturn, for implementing call probes in the debugger

## 2026-09-01
* renamed didReturn to didReturnFromSubroutine and add didJumpToSubroutine
* fix the absolutely horrendous state of addrspace.cpp. move translate code into translate method, add cached mappings so we don't have to iterate throuh every mapping, add common read, peek and write methods which handles calling getmapping (or peekmapping), add peekmapping so cachedmapping isn't corrupted by the debugger and generally optimise the syntax
* fix incorrect MMU pagetable masking. This gets us past "md0b"
* fix rtc incorrect control register read. NOW WE BOOT!!!!!!
* add fakeGF2 convar to fake a gf2 board.
* make failing to open hdd a message instead of a error
* fix launcher
* allow ability to set default switch state
* v0.2.0-rc1 RELEASE !!! If everything good I will spin again
    Didn't do some things

## 2026-09-03
* start splitting off reset from shutdown 
* get rid of ridiculous hack where we manually bitbanged the address into the memory and make it machine specific

## 2026-09-04 to 2026-09-05
* made reset much less of an invasive process
    * it doesn't shut down and reinitialise the emualtor but just runs the reset method fo all components
    
v0.3.0 TODO:

* GE and FBC!!!!!!!
* Reset architecture is garbage and causes enormous memory corruption. Shutdown code is more like ShitDown and would require probably a rewrite (?) to not constantly break, so let's do a non-invasive reset by adding Reset. UPDATE WHEN ACTUALLY DONE
* Fix UC4 double buffering
* HiDPI support
* Abstract away initial PC
* proper stack handling
