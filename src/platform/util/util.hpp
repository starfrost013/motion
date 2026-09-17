/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    util.hpp: Various misc. utilities
*/

#pragma once
#include <Motion.hpp>

// ALL UTIL INCLUDES MUST BE HERE

namespace Motion
{
    class Util
    {
        #define Chrono_GetTime() std::chrono::steady_clock::now().time_since_epoch()
        #define Chrono_GetTicksMS(x) std::chrono::duration_cast<std::chrono::milliseconds>(x).count()
        #define Chrono_GetTicksMSElapsed(start, end) std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        #define Chrono_GetTicksUS(x) std::chrono::duration_cast<std::chrono::microseconds>(x).count()
        #define Chrono_GetTicksUSElapsed(start, end) std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
        #define Chrono_GetTicksNS(x) std::chrono::duration_cast<std::chrono::nanoseconds>(x).count()
        #define Chrono_GetTicksNSElapsed(start, end) std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
    };
};