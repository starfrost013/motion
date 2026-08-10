# 
#   m  o  t  i  o  n
#   The SGI Emulator
#
#   Copyright (c)2026 starfrost
#
#   CMakeLists.txt: Raw buildscript.
#

cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# $<CONFIG> rather than ${CMAKE_BUILD_TYPE}: specify with -DCMAKE_BUILD_TYPE
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/$<CONFIG>)

# mingw-w64 doesn't support static linking against the UCRT

# Install path
set(ROM_SRC_DIR "${CMAKE_SOURCE_DIR}/roms")
set(ROM_DST_DIR "${CMAKE_BINARY_DIR}/output/$<CONFIG>/roms")
set(ASSET_SRC_DIR "${CMAKE_SOURCE_DIR}/assets")
set(ASSET_DST_DIR "${CMAKE_BINARY_DIR}/output/$<CONFIG>/")

project(motion)

# Build SDL static, we like static libraries (esp. on Linux, where the system SDL3 might not be the same version!)

set(SDL_SHARED OFF CACHE BOOL "Build SDL shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL static library" FORCE)

# Add our submodule directories

add_subdirectory(external/sdl3 EXCLUDE_FROM_ALL)

# Also allow us to use our own headers. At some point, we need to have a public/private header distinction...
add_subdirectory(src)

# Setup our build type info
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(motion PRIVATE DEBUG)
    # Turn on ASan, O1 level optimisation. -fno-omit-frame-pointer gives us 
    target_compile_options(motion PRIVATE -fsanitize=address -O2 -fno-omit-frame-pointer -fno-inline -U_FORTIFY_SOURCE)
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    target_compile_definitions(motion PRIVATE RELEASE)
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    target_compile_definitions(motion PRIVATE RELEASE)
    target_compile_definitions(motion PRIVATE RELEASE_WITH_DEBUG_INFO)
endif()

if(WIN32 AND MINGW)
    target_link_options(motion PRIVATE 
        -static -static-libgcc -static-libstdc++ 
        -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
    )
elseif(APPLE)
    # Apple Clang hard-errors because libc++ is part of MacOS ABI
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # if you statically link everything basically pipewrie will KILL SDL lol
    target_link_options(motion PRIVATE -static-libstdc++ -static-libgcc)             # static stdc++

    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        target_link_options(motion PRIVATE -fsanitize=address)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang on Linux/BSD: -static-libstdc++ is fine, -static-libgcc is not reliable
    target_link_options(motion PRIVATE -static-libstdc++)

    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        target_link_options(motion PRIVATE -fsanitize=address)
    endif()
endif()

target_link_libraries(motion PRIVATE 
SDL3::SDL3-static                   # static sdl3
moira)                              # 68k

# install
add_custom_target(InstallAssets 
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${ROM_SRC_DIR}" "${ROM_DST_DIR}"
    COMMENT "Updating ROMs..."

    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${ASSET_SRC_DIR}" "${ASSET_DST_DIR}"
    COMMENT "Updating ROMs..."
)

add_dependencies(motion InstallAssets)

# Flat so PROM is loaded from a path relative to the cwd
install(TARGETS motion RUNTIME DESTINATION .)

# On by default for releases, turn it off with -DMOTION_INSTALL_ROMS=OFF.
option(MOTION_INSTALL_ROMS "Bundle roms/ into the install tree" ON)

if(MOTION_INSTALL_ROMS)
    install(DIRECTORY "${ROM_SRC_DIR}/" DESTINATION roms
        PATTERN ".DS_Store" EXCLUDE
        PATTERN "Thumbs.db" EXCLUDE
    )
endif()