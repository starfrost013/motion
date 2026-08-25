# IRIS - A 68000 SGI Emulator!
# Copyright (C) 2026 starfrost

# Main /src directory makefile

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/external/sdl3/include)
include_directories(${CMAKE_SOURCE_DIR}/external/imgui)

add_subdirectory(${CMAKE_SOURCE_DIR}/src/component/cpu/moira)

add_executable(motion

    # Dear Imgui
    ../external/imgui/imgui_draw.cpp
    ../external/imgui/imgui_tables.cpp
    ../external/imgui/imgui_widgets.cpp
    ../external/imgui/imgui.cpp
    ../external/imgui/backends/imgui_impl_sdl3.cpp
    ../external/imgui/backends/imgui_impl_sdlgpu3.cpp

    # Motion Emulator
    main.cpp
    base/emulation.cpp
    base/machine/machine.cpp
    base/filesystem/filesystem.cpp
    base/cvar/cvar.cpp
    component/component.cpp
    component/addrspace.cpp
    component/memory.cpp
    component/cpu/mc68020_core.cpp

    coherent/coherent_core.cpp
    coherent/coherent_gui_main.cpp
    render/sdl3/render_sdl3_core.cpp
    component/ip2/prom.cpp
    coherent/coherent_gui_logging.cpp
    component/cpu/mc68020_debug.cpp
    component/ip2/ip2_interrupt.cpp
    component/ip2/ip2_mmu.cpp
    component/ip2/ip2_duart.cpp
    component/ip2/ip2_rtc.cpp
    component/ip2/ip2_mouse.cpp
    component/ip2/ip2_mmu_debug.cpp
    coherent/coherent_gui_imgui.cpp
    coherent/coherent_gui_style.cpp
    base/cmdline/cmdline.cpp
    component/ip2/ip2_duart_debug.cpp
    component/serial/serial.hpp
    component/serial/serial.cpp
    component/ip2/ip2_dip_switches.cpp
    base/event/event.cpp
    base/profile/profile.cpp
    component/mmu/mmu.cpp
    component/multibus/multibus.cpp
    coherent/coherent_system.cpp
    component/keyboard/keyboard_iris.cpp
    component/multibus/multibus_debug.cpp
    render/sdl3/render_sdl3_texture.cpp
    render/render_core.cpp
    render/sdl3/render_sdl3_passes.cpp
    render/sdl3/render_sdl3_debug.cpp

    # "Juniper" (IRIS 2000T / 3000) graphics system
    component/gpu/juniper/uc4/uc4_core.cpp
    component/gpu/juniper/dc4/dc4_core.cpp
    component/gpu/juniper/dc4/dc4_debug.cpp
    component/gpu/juniper/bp3/bp3_core.cpp
    component/gpu/juniper/gf2/gf2_core.cpp
    component/gpu/juniper/gf2/gf2_commands.cpp
    component/gpu/juniper/gf2/gf2_geometry.cpp
    component/gpu/vram.cpp
    render/sdl3/render_sdl3_window.cpp
    base/machine/machine_iris3130.cpp
    component/gpu/juniper/uc4/uc4_debug.cpp
    component/gpu/juniper/uc4/uc4_commands.cpp
    coherent/coherent_editor.cpp
    launcher/launcher.cpp
    component/storage/dsd5217.cpp
    base/profile/profile_cvars.cpp
    component/storage/dsd5217_debug.cpp
    component/storage/storager2.cpp
    base/filesystem/disk_image.cpp
)

