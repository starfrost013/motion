/*
    "Lisburn" 68020 core (just down the road from Moira)
    A fork of Moira
    Copyright (c) 2019-2025 Dirk W. Hoffmann. www.dirkwhoffmann.de
    Copyright (c) 2026 Connor Hyde (starfrost). www.starfrost.net

    Published under the terms of the MIT license
*/

#pragma once

/* Set to true to enable precise timing mode (68000 and 68010 only).
 *
 * When disabled, Moira calls the 'sync' function at the end of each instruction,
 * passing the number of elapsed cycles as an argument. In precise timing mode,
 * 'sync' is called before each memory access, allowing the client to emulate
 * the surrounding hardware up to the point where the memory access occurs.
 *
 * Precise timing mode is available only for 68000 and 68010 emulation. For
 * other supported models, this setting has no effect.
 *
 * Enable to improve accuracy, disable it to enhance performance.
 */
#define MOIRA_PRECISE_TIMING false

/* Set to true to enable address error checking.
 *
 * The 68000 and 68010 trigger an address error violation if a word or longword
 * is accessed at an odd memory address.
 *
 * Enable to improve accuracy, disable to gain speed.
 */
#define MOIRA_EMULATE_ADDRESS_ERROR false

/* Set to true to emulate function code pins FC0 - FC2.
 *
 * Whenever memory is accessed, the function code pins enable external hardware
 * to inspect the access type. If used, these pins are usually connected to an
 * external memory management unit (MMU).
 *
 * Enable to improve accuracy, disable to gain speed.
 */
#define MOIRA_EMULATE_FC true

/* The following macro appears at the beginning of each instruction handler.
 * Moira will call 'willExecute(...)' for all listed instructions.
 */
#define MOIRA_WILL_EXECUTE    I == Instr::STOP || I == Instr::TAS || I == Instr::BKPT

/* The following macro appear at the end of each instruction handler.
 * Moira will call 'didExecute(...)' for all listed instructions.
 */
#define MOIRA_DID_EXECUTE     I == Instr::RESET

/* Controls assertion checking.
 *
 * Comment out to enable assertions (recommended for debugging).
 * Uncomment to disable assertions (recommended for release builds).
 */
#include <cassert>
