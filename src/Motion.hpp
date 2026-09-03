/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    Motion.hpp: Main file for motion (previously called Iris, but it was too generic) SGI Emulator
*/

#pragma once

// Standard includes
#include <any>
#include <cctype>
#include <chrono>
#include <concepts>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <format>
#include <fstream>
#include <queue>
#include <vector>
#include <unordered_map>
#include <thread>

// should be included everywhere as it's standalone
#include <platform/logging/logging.hpp>
#include <platform/util/util.hpp>

// Not standalone, but useful for everything
#include <base/cvar/cvar.hpp>
#include <base/cmdline/cmdline.hpp>

#define APP_NAME            "motion"
#define APP_SIGNON          "The SGI Emulator\nEmulation engine © 2026 starfrost\nOriginal hardware and software by Silicon Graphics, Inc. © 1981-1989"
// This part will be replaced by some fancy GHA script later
#define APP_VERSION         "0.2.0"
#ifdef DEBUG
#define APP_NATURE          "Debug"
#elif RELEASE_WITH_DEBUG_INFO
#define APP_NATURE          "Release with Debug Info"
#elif RELEASE
#define APP_NATURE          "Release"
#else 
#define APP_NATURE          "????????"
#endif

#define APP_BUILD_DATE      "@ " __DATE__ " " __TIME__

// Exit codes
#define EXIT_SUCCESS        0
#define EXIT_FAILURE        1

// Log prefixes (these will all be moved)

#define ARRAY_ELEMS(x)       sizeof(x)/sizeof(x[0])

// Some string lengths
#define STRING_MAX_SHORT    48
#define STRING_MAX_LONG     256
#define STRING_MAX_PATH     260
                                        
// Lets the programmer know if we are deliberately allowing a falltrhough            
#define fallthrough

// Flips the state of a mask
#define FLIP_MASK_STATE(dst, cond)              if (dst & cond) \
                                                    dst &= ~(cond); \
                                                else \
                                                    dst |= cond
// Endianness shit

#define TOBE16(value)                           value = (value >> 8) | (value << 8)

#define TOBE32(value)                           value = ((((value) & 0xff000000) >> 24)| \
                                                (((value) & 0x00ff0000) >> 8) | \
                                                (((value) & 0x0000ff00) << 8) | \
                                                (((value) & 0x000000ff) << 24))                                  

// Assertions.
// I usually don't like them except in critical-path stuff, where it may be too slow to do manual checks.

#ifdef DEBUG
#define MOTION_ASSERT(cond, msg)                if (cond) \
                                                    Logger::Log(std::format("***** ASSERTION FAILED *****\n{}", msg).c_str(), LogChannels::FatalError) 
#define MOTION_ASSERT_UNSAFE(cond, msg)         if (cond) \
                                                    Logger::Log(std::format("***** ASSERTION FAILED *****\n{}", msg).c_str(), LogChannels::UnsafeShutdown)
#else
#define MOTION_ASSERT(cond, msg)
#define MOTION_ASSERT_UNSAFE(cond, msg)
#endif

// Memory I/O macros for unaligned address access support

#define READ16_BE(space, addr)                  (uint16_t)(((uint16_t)space[addr] << 8 \
                                                        | space[(addr + 1)])) // % mask to prevent issues

#define READ32_BE(space, addr)                  ((uint32_t)space[addr] << 24) \
                                                        | ((uint32_t)space[addr + 1] << 16) \
                                                        | ((uint32_t)space[addr + 2] << 8) \
                                                        | (uint32_t)space[addr + 3]

#define WRITE16_BE(space, addr, value)          space[addr] = (uint8_t)(value >> 8); \
                                                        space[addr + 1] = (uint8_t)value

#define WRITE32_BE(space, addr, value)          space[addr] = (uint8_t)(value >> 24); \
                                                        space[addr + 1] = (uint8_t)(value >> 16); \
                                                        space[addr + 2] = (uint8_t)(value >> 8); \
                                                        space[addr + 3] = (uint8_t)value
