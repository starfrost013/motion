/*
    "Lisburn" 68020 core (just down the road from Moira)
    A fork of Moira
    Copyright (c) 2019-2025 Dirk W. Hoffmann. www.dirkwhoffmann.de
    Copyright (c) 2026 Connor Hyde (starfrost). www.starfrost.net

    Published under the terms of the MIT license
*/

//
// Auxiliary functions
//

// Reads a data value from memory without side-effects
template <Size S = Word> u32 dasmRead(u32 addr) const;

// Increments addr and reads a data value from memory without side-effects
template <Size S = Word> u32 dasmIncRead(u32 &addr) const;

// Assembles an operand
template <Mode M, Size S = Word> Ea<M, S>Op(u16 reg, u32 &pc) const;
