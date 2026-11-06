/*
    "Lisburn" 68020 core (just down the road from Moira)
    A fork of Moira
    Copyright (c) 2019-2025 Dirk W. Hoffmann. www.dirkwhoffmann.de
    Copyright (c) 2026 Connor Hyde (starfrost). www.starfrost.net

    Published under the terms of the MIT license
*/

#include <cmath>

static const char *mnemonics[]
{
    // 68000
    "abcd" ,    "add",      "adda",     "addi",     "addq",     "addx",
    "and",      "andi",     "andi",     "andi",     "asl",      "asr",
    "bcc",      "bcs",      "beq",      "bge",      "bgt",      "bhi",
    "ble",      "bls",      "blt",      "bmi",      "bne",      "bpl",
    "bvc",      "bvs",      "bchg",     "bclr",     "bra",      "bset",
    "bsr",      "btst",     "chk",      "clr",      "cmp",     "cmpa",
    "cmpi",     "cmpm",     "dbcc",    "dbcs",      "dbeq",     "dbge",
    "dbgt",     "dbhi",     "dble",     "dbls",     "dblt",     "dbmi",
    "dbne",     "dbpl",     "dbvc",     "dbvs",     "dbra",     "dbt",
    "divs",     "divu",     "eor",      "eori",     "eori",     "eori",
    "exg",      "ext",      "???",      "jmp",      "jsr",      "lea",
    "???",      "???",      "link",     "lsl",      "lsr",      "move",
    "movea",    "move",     "move",     "move",     "move",     "move",
    "movem",    "movep",    "moveq",    "muls",     "mulu",     "nbcd",
    "neg",      "negx",     "nop",      "not",      "or",       "ori",
    "ori",      "ori",      "pea",      "reset",    "rol",      "ror",
    "roxl",     "roxr",     "rte",      "rtr",      "rts",      "sbcd",
    "scc",      "scs",      "seq",      "sge",      "sgt",      "shi",
    "sle",      "sls",      "slt",      "smi",      "sne",      "spl",
    "svc",      "svs",      "sf",       "st",       "stop",     "sub",
    "suba",     "subi",     "subq",     "subx",     "swap",     "tas",
    "trap",     "trapv",    "tst",      "unlk",

    // 68010
    "bkpt",     "movec",    "moves",    "rtd",

    // 68020
    "bfchg",    "bfclr",    "bfexts",   "bfextu",   "bfffo",    "bfins",
    "bfset",    "bftst",    "callm",    "cas",      "cas2",     "chk2",
    "cmp2",     "b",        "db",       "gen",      "restore",  "save",
    "s",        "trap",     "divl",     "extb",     "mull",     "pack",
    "rtm",      "trapcc",   "trapcs",   "trapeq",   "trapge",   "trapgt",
    "traphi",   "traple",   "trapls",   "traplt",   "trapmi",   "trapne",
    "trappl",   "trapvc",   "trapvs",   "trapf",    "trapt",    "unpk",

    // 68040
    "cinv",     "cpush",    "move16",

    // MMU
    "pflush",   "pflusha",  "pflushan", "pflushn",  "pload",    "pmove",    "ptest",

    // FPU
    "fabs",     "fadd",     "fb",       "fcmp",     "fdb",      "fdiv",
    "fmove",    "fmovem",   "fmul",     "fneg",     "fnop",     "frestore",
    "fsave",    "fs",       "fsqrt",    "fsub",     "ftrap",    "ftst",

    "fsabs",    "fdabs",    "fsadd",    "fdadd",    "fsdiv",    "fddiv",
    "fsmove",   "fdmove",   "fsmul",    "fdmul",    "fsneg",    "fdneg",
    "fssqrt",   "fdsqrt",   "fssub",    "fdsub",

    "facos",    "fasin",    "fatan",    "fatanh",   "fcos",     "fcosh",
    "fetox",    "fetoxm1",  "fgetexp",  "fgetman",  "fint",     "fintrz",
    "flog10",   "flog2",    "flogn",    "flognp1",  "fmod",     "fmovecr",
    "frem",     "fscale",   "fsgldiv",  "fsglmul",  "fsin",     "fsincos",
    "fsinh",    "ftan",     "ftanh",    "ftentox",  "ftwotox"
};

static int decDigits(u64 value) { return value ? 1 + (int)log10(value) : 1; }
static int binDigits(u64 value) { return value ? 1 + (int)log2(value) : 1; }
static int hexDigits(u64 value) { return (binDigits(value) + 3) / 4; }

static void sprintd(char *&s, u64 value, int digits)
{
    for (int i = digits - 1; i >= 0; i--) {

        u8 digit = value % 10;
        s[i] = '0' + digit;
        value /= 10;
    }
    s += digits;
}

static void sprintd(char *&s, u64 value)
{
    sprintd(s, value, decDigits(value));
}

static void sprintd_signed(char *&s, i64 value)
{
    if (value < 0) { *s++ = '-'; value *= -1; }
    sprintd(s, value, decDigits(value));
}

static void sprintx(char *&s, u64 value, const DasmNumberFormat &fmt, int digits)
{
    char a = (fmt.upperCase ? 'A' : 'a') - 10;

    if (value || !fmt.plainZero) {

        for (int i = 0; fmt.prefix[i]; i++) *s++ = fmt.prefix[i];
    }
    for (int i = digits - 1; i >= 0; i--) {

        auto digit = char(value % 16);
        s[i] = (digit <= 9) ? ('0' + digit) : (a + digit);
        value /= 16;
    }
    s += digits;
}

static void sprintx(char *&s, u64 value, const DasmNumberFormat &fmt)
{
    sprintx(s, value, fmt, hexDigits(value));
}

static void sprintx_signed(char *&s, i64 value, const DasmNumberFormat &fmt)
{
    if (value < 0) { *s++ = '-'; value *= -1; }
    sprintx(s, value, fmt, hexDigits(value));
}

static void sprint(char *&s, u64 value, const DasmNumberFormat &fmt)
{
    fmt.radix == 10 ? sprintd(s, value) : sprintx(s, value, fmt);
}

static void sprint_signed(char *&s, i64 value, const DasmNumberFormat &fmt)
{
    fmt.radix == 10 ? sprintd_signed(s, value) : sprintx_signed(s, value, fmt);
}

StrWriter&
StrWriter::operator<<(char c)
{
    *ptr++ = c;
    return *this;
}

StrWriter&
StrWriter::operator<<(const char *str)
{
    while (*str) { *ptr++ = *str++; };
    return *this;
}

StrWriter&
StrWriter::operator<<(int value)
{
    sprintd_signed(ptr, value);
    return *this;
}

StrWriter&
StrWriter::operator<<(unsigned int value)
{
    sprintd(ptr, value);
    return *this;
}

StrWriter&
StrWriter::operator<<(Int i)
{
    sprint_signed(ptr, i.raw, style.numberFormat);
    return *this;
}

StrWriter&
StrWriter::operator<<(UInt u)
{
    sprint(ptr, u.raw, style.numberFormat);
    return *this;
}

StrWriter&
StrWriter::operator<<(UInt8 u)
{
    if (style.numberFormat.radix == 16) {
        sprintx(ptr, u.raw, style.numberFormat, 2);
    } else {
        sprintd(ptr, u.raw, 3);
    }
    return *this;
}

StrWriter&
StrWriter::operator<<(UInt16 u)
{
    if (style.numberFormat.radix == 16) {
        sprintx(ptr, u.raw, style.numberFormat, 4);
    } else {
        sprintd(ptr, u.raw, 5);
    }
    return *this;
}

StrWriter&
StrWriter::operator<<(UInt24 u)
{
    if (style.numberFormat.radix == 16) {
        sprintx(ptr, u.raw, style.numberFormat, 6);
    } else {
        sprintd(ptr, u.raw, 8);
    }
    return *this;
}

StrWriter&
StrWriter::operator<<(UInt32 u)
{
    if (style.numberFormat.radix == 16) {
        sprintx(ptr, u.raw, style.numberFormat, 8);
    } else {
        sprintd(ptr, u.raw, 10);
    }
    return *this;
}

template <Size S> StrWriter&
StrWriter::operator<<(Imu<S> im)
{
    *ptr++ = '#';
    *this << UInt(CLIP<S>(im.raw));
    return *this;
}

template <Size S> StrWriter&
StrWriter::operator<<(Ims<S> im)
{
    *ptr++ = '#';
    *this << Int(SEXT<S>(im.raw));
    return *this;
}

StrWriter&
StrWriter::operator<<(Imd im)
{
    *ptr++ = '#';
    sprintd(ptr, im.raw);
    return *this;
}

template <Instr I> StrWriter&
StrWriter::operator<<(Ins<I> i)
{
    if constexpr (I == Instr::DBF) {
        *this << "dbra";

    } else {
        *this << mnemonics[(int)I];
    }

    return *this;
}

template <Size S> StrWriter&
StrWriter::operator<<(Sz<S>)
{
    *this << ((S == Byte) ? ".b" : (S == Word) ? ".w" : ".l");
    
    return *this;
}

template <Size S> StrWriter&
StrWriter::operator<<(Szb<S>)
{
    *ptr++ = '.';

    return *this;
}

StrWriter&
StrWriter::operator<<(Cc cc)
{
    switch (Cond(cc.raw)) {

        case Cond::BT: *this << "t";   break;
        case Cond::BF: *this << "f";   break;
        case Cond::HI: *this << "hi";  break;
        case Cond::LS: *this << "ls";  break;
        case Cond::CC: *this << "cc";  break;
        case Cond::CS: *this << "cs";  break;
        case Cond::NE: *this << "ne";  break;
        case Cond::EQ: *this << "eq";  break;
        case Cond::VC: *this << "vc";  break;
        case Cond::VS: *this << "vs";  break;
        case Cond::PL: *this << "pl";  break;
        case Cond::MI: *this << "mi";  break;
        case Cond::GE: *this << "ge";  break;
        case Cond::LT: *this << "lt";  break;
        case Cond::GT: *this << "gt";  break;
        case Cond::LE: *this << "le";  break;
    }
    return *this;
}

StrWriter&
StrWriter::operator<<(Cpcc cpcc)
{
    switch (cpcc.raw) {

        case 0:  *this << "f";      break;
        case 1:  *this << "eq";     break;
        case 2:  *this << "ogt";    break;
        case 3:  *this << "oge";    break;
        case 4:  *this << "olt";    break;
        case 5:  *this << "ole";    break;
        case 6:  *this << "ogl";    break;
        case 7:  *this << "or";     break;
        case 8:  *this << "un";     break;
        case 9:  *this << "ueq";    break;
        case 10: *this << "ugt";    break;
        case 11: *this << "uge";    break;
        case 12: *this << "ult";    break;
        case 13: *this << "ule";    break;
        case 14: *this << "ne";     break;
        case 15: *this << "t";      break;
        case 16: *this << "sf";     break;
        case 17: *this << "seq";    break;
        case 18: *this << "gt";     break;
        case 19: *this << "ge";     break;
        case 20: *this << "lt";     break;
        case 21: *this << "le";     break;
        case 22: *this << "gl";     break;
        case 23: *this << "gle";    break;
        case 24: *this << "ngle";   break;
        case 25: *this << "ngl";    break;
        case 26: *this << "nle";    break;
        case 27: *this << "nlt";    break;
        case 28: *this << "nge";    break;
        case 29: *this << "ngt";    break;
        case 30: *this << "sne";    break;
        case 31: *this << "st";     break;

        default:
            *this << "?";
    }
    return *this;
}

StrWriter&
StrWriter::operator<<(Fcc fcc)
{
    switch (fcc.raw & 0x1F) {

        case 0:  *this << "f";      break;
        case 1:  *this << "eq";     break;
        case 2:  *this << "ogt";    break;
        case 3:  *this << "oge";    break;
        case 4:  *this << "olt";    break;
        case 5:  *this << "ole";    break;
        case 6:  *this << "ogl";    break;
        case 7:  *this << "or";     break;
        case 8:  *this << "un";     break;
        case 9:  *this << "ueq";    break;
        case 10: *this << "ugt";    break;
        case 11: *this << "uge";    break;
        case 12: *this << "ult";    break;
        case 13: *this << "ule";    break;
        case 14: *this << "ne";     break;
        case 15: *this << "t";      break;
        case 16: *this << "sf";     break;
        case 17: *this << "seq";    break;
        case 18: *this << "gt";     break;
        case 19: *this << "ge";     break;
        case 20: *this << "lt";     break;
        case 21: *this << "le";     break;
        case 22: *this << "gl";     break;
        case 23: *this << "gle";    break;
        case 24: *this << "ngle";   break;
        case 25: *this << "ngl";    break;
        case 26: *this << "nle";    break;
        case 27: *this << "nlt";    break;
        case 28: *this << "nge";    break;
        case 29: *this << "ngt";    break;
        case 30: *this << "sne";    break;
        case 31: *this << "st";     break;
    }
    return *this;
}

StrWriter&
StrWriter::operator<<(Pcc pcc)
{
    switch (pcc.raw) {

        case 0:  *this << "bs"; break;
        case 1:  *this << "bc"; break;
        case 2:  *this << "ls"; break;
        case 3:  *this << "lc"; break;
        case 4:  *this << "ss"; break;
        case 5:  *this << "sc"; break;
        case 6:  *this << "as"; break;
        case 7:  *this << "ac"; break;
        case 8:  *this << "ws"; break;
        case 9:  *this << "wc"; break;
        case 10: *this << "is"; break;
        case 11: *this << "ic"; break;
        case 12: *this << "gs"; break;
        case 13: *this << "gc"; break;
        case 14: *this << "cs"; break;
        case 15: *this << "cc"; break;

        default:
            *this << "?";
    }
    return *this;
}

StrWriter&
StrWriter::operator<<(Dn dn)
{
    *ptr++ = 'D'; *ptr++ = '0' + (char)dn.raw;
    return *this;
}

StrWriter&
StrWriter::operator<<(An an)
{
    *ptr++ = 'A'; *ptr++ = '0' + (char)an.raw;
    return *this;
}

StrWriter&
StrWriter::operator<<(Rn rn)
{
    rn.raw < 8 ? *this << Dn{rn.raw} : *this << An{rn.raw - 8};
    return *this;
}

StrWriter&
StrWriter::operator<<(Ccr _)
{
    *ptr++ = 'C'; *ptr++ = 'C'; *ptr++ = 'R';

    return *this;
}

StrWriter&
StrWriter::operator<<(Pc _)
{
    *ptr++ = 'P'; *ptr++ = 'C';
    return *this;
}

StrWriter&
StrWriter::operator<<(Zpc _)
{
    *ptr++ = 'Z'; *ptr++ = 'P'; *ptr++ = 'C';
    return *this;
}

StrWriter&
StrWriter::operator<<(Sr _)
{
    *ptr++ = 'S'; *ptr++ = 'R';
    return *this;
}

StrWriter&
StrWriter::operator<<(Usp _)
{
    *ptr++ = 'U'; *ptr++ = 'S'; *ptr++ = 'P';
    return *this;
}

StrWriter&
StrWriter::operator<<(Cn cn)
{
    bool valid = cn.raw <= 0x007 || (cn.raw >= 0x800 && cn.raw <= 0x807);
    bool upper = true;

    if (valid) {
        switch (cn.raw) {

            case 0x000: *this << ("SFC");   break;
            case 0x001: *this << ("DFC");   break;
            case 0x002: *this << ("CACR");  break;
            case 0x003: *this << ("TC");    break;
            case 0x004: *this << ("ITT0");  break;
            case 0x005: *this << ("ITT1");  break;
            case 0x006: *this << ("DTT0");  break;
            case 0x007: *this << ("DTT1");  break;
            case 0x008: *this << ("BUSCR"); break;
            case 0x800: *this << ("USP");   break;
            case 0x801: *this << ("VBR");   break;
            case 0x802: *this << ("CAAR");  break;
            case 0x803: *this << ("MSP");   break;
            case 0x804: *this << ("ISP");   break;
            case 0x805: *this << ("MMUSR"); break;
            case 0x806: *this << ("URP");   break;
            case 0x807: *this << ("SRP");   break;
            case 0x808: *this << ("PCR");   break;
        }

    } else {
        *this << "INVALID";
    }

    return *this;
}

StrWriter&
StrWriter::operator<<(RegList l)
{
    int r[16];

    // Step 1: Fill array r with the register list bits, e.g., 11101101
    for (int i = 0; i <= 15; i++) { r[i] = !!(l.raw & (1 << i)); }

    // Step 2: Convert 11101101 to 12301201
    for (int i = 1; i <= 15; i++) { if (r[i]) r[i] = r[i-1] + 1; }

    // Step 3: Convert 12301201 to 33302201
    for (int i = 14; i >= 0; i--) { if (r[i] && r[i+1]) r[i] = r[i+1]; }

    // Step 4: Convert 33302201 to "D0-D2/D4/D5/D7"
    bool first = true;
    for (int i = 0; i <= 15; i += r[i] + 1) {

        if (r[i] == 0) continue;

        // Print delimiter
        if (first) { first = false; } else { *this << "/"; }

        // Format variant 1: Single register
        if (r[i] == 1) { *this << Rn{i}; }

        // Format variant 2: Register range
        else { *this << Rn{i} << "-" << Rn{i+r[i]-1}; }
    }

    return *this;
}

StrWriter&
StrWriter::operator<<(RegRegList l)
{
    u16 regsD = l.raw & 0x00FF;
    u16 regsA = l.raw & 0xFF00;

    *this << RegList { regsD };
    if (regsD && regsA) *this << "/";
    *this << RegList { regsA };

    return *this;
}

StrWriter&
StrWriter::operator<<(FRegList l)
{
    int r[8];

    // Step 1: Fill array r with the register list bits, e.g., 11101101
    for (int i = 0; i <= 7; i++) { r[i] = !!(l.raw & (1 << i)); }

    // Step 2: Convert 11101101 to 12301201
    for (int i = 1; i <= 7; i++) { if (r[i]) r[i] = r[i-1] + 1; }

    // Step 3: Convert 12301201 to 33302201
    for (int i = 6; i >= 0; i--) { if (r[i] && r[i+1]) r[i] = r[i+1]; }

    // Step 4: Convert 33302201 to "FP0-FP2/FP4/FP5/FP7"
    bool first = true;
    for (int i = 0; i <= 7; i += r[i] + 1) {

        if (r[i] == 0) continue;

        // Print delimiter
        if (first) { first = false; } else { *this << "/"; }

        // Format variant 1: Single register
        if (r[i] == 1) { *this << Fp{i}; }

        // Format variant 2: Register range
        else { *this << Fp{i} << "-" << Fp{i+r[i]-1}; }
    }

    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(const Ea<M, S> &ea)
{
    switch (M) {

        case Mode::DN:   *this << Dn{ea.reg};    break;
        case Mode::AN:   *this << An{ea.reg};    break;
        case Mode::AI:   *this << Ai<M,S>{ea};   break;
        case Mode::PI:   *this << Pi<M,S>{ea};   break;
        case Mode::PD:   *this << Pd<M,S>{ea};   break;
        case Mode::DI:   *this << Di<M,S>{ea};   break;
        case Mode::IX:   *this << Ix<M,S>{ea};   break;
        case Mode::AW:   *this << Aw<M,S>{ea};   break;
        case Mode::AL:   *this << Al<M,S>{ea};   break;
        case Mode::DIPC: *this << DiPc<M,S>{ea}; break;
        case Mode::IXPC: *this << Ix<M,S>{ea};   break;
        case Mode::IM:   *this << Im<M,S>{ea};   break;

        default:
            *this << "???";
    }
    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(Ai<M, S> wrapper)
{
    auto &ea = wrapper.ea;
    *this << "(" << An{ea.reg} << ")";

    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(Pi<M, S> wrapper)
{
    auto &ea = wrapper.ea;
    *this << "(" << An{ea.reg} << ")+";
    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(Pd<M, S> wrapper)
{
    auto &ea = wrapper.ea;
    *this << "-(" << An{ea.reg} << ")";
    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(Di<M, S> wrapper)
{
    auto &ea = wrapper.ea;
    *this << "(" << Int{(i16)ea.ext1} << "," << An{ea.reg} << ")";
    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(Ix<M, S> wrapper)
{
    *this << IxMot<M, S>{wrapper.ea};

    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(IxMot<M, S> wrapper)
{
    assert(M == Mode(6) || M == Mode(10));

    auto &ea = wrapper.ea;

    u16 full = _______x________ (ea.ext1);

    if (!full) {

        //   15-12   11   10   09   08   07   06   05   04   03   02   01   00
        // --------------------------------------------------------------------
        // | REG   | LW | SCALE   | 0  | DISPLACEMENT                         |
        // --------------------------------------------------------------------

        u16 reg   = xxxx____________ (ea.ext1);
        u16 lw    = ____x___________ (ea.ext1);
        u16 scale = _____xx_________ (ea.ext1);
        u16 disp  = ________xxxxxxxx (ea.ext1);

        *this << "(" << Int{(i8)disp} << ",";
        M == Mode(10) ? *this << Pc{} : *this << An{ea.reg};
        *this << "," << Rn{reg} << (lw ? ".l" : ".w") << Scale{scale} << ")";

    } else {

        //   15-12   11   10   09   08   07   06   05   04   03   02   01   00
        // --------------------------------------------------------------------
        // | REG   | LW | SCALE   | 1  | BS | IS | BD SIZE  | 0  | IIS        |
        // --------------------------------------------------------------------

        u16  reg   = xxxx____________ (ea.ext1);
        u16  lw    = ____x___________ (ea.ext1);
        u16  scale = _____xx_________ (ea.ext1);
        u16  bs    = ________x_______ (ea.ext1);
        u16  is    = _________x______ (ea.ext1);
        u16  size  = __________xx____ (ea.ext1);
        u16  iis   = _____________xxx (ea.ext1);
        u32  base  = ea.ext2;
        u32  outer = ea.ext3;

        auto baseDisplacement = [&]() {
            size == 3 ? (*this << Int{(i32)base}) : (*this << Int{(i16)base});
        };
        auto baseRegister = [&]() {
            if constexpr (M == Mode(10)) {
                if (!bs) { *this << Sep{} << Pc{}; } else { *this << Sep{} << Zpc{}; }
            } else {
                if (!bs) { *this << Sep{} << An{ea.reg}; }
            }
        };
        auto indexRegister = [&]() {
            if (!is) *this << Sep{} << Rn{reg} << (lw ? ".l" : ".w") << Scale{scale};
        };
        auto outerDisplacement = [&]() {
            *this << Sep{} << Int(outer);
        };

        if (iis == 0) {

            // Memory Indirect Unindexed
            *this << "(";
            baseDisplacement(); baseRegister(); indexRegister();
            *this << ")";

        } else if (iis & 0b100) {

            // Memory Indirect Postindexed
            *this << "([";
            baseDisplacement(); baseRegister();
            *this << "]";
            indexRegister(); outerDisplacement();
            *this << ")";

        } else {

            // Memory Indirect Preindexed
            *this << "([";
            baseDisplacement(); baseRegister(); indexRegister();
            *this << "]";
            outerDisplacement();
            *this << ")";
        }
    }

    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(IxMit<M, S> wrapper)
{
    assert(M == Mode(6) || M == Mode(10));

    auto &ea = wrapper.ea;

    u16 full = _______x________ (ea.ext1);

    if (!full) {

        //   15-12   11   10   09   08   07   06   05   04   03   02   01   00
        // --------------------------------------------------------------------
        // | REG   | LW | SCALE   | 0  | DISPLACEMENT                         |
        // --------------------------------------------------------------------

        u16 reg   = xxxx____________ (ea.ext1);
        u16 lw    = ____x___________ (ea.ext1);
        u16 scale = _____xx_________ (ea.ext1);
        u16 disp  = ________xxxxxxxx (ea.ext1);

        M == Mode(10) ? *this << Pc{} : *this << An{ea.reg};
        *this << "@(" << Int{(i8)disp};
        *this << "," << Rn{reg} << (lw ? ":l" : ":w") << Scale{scale} << ")";

    } else {

        //   15-12   11   10   09   08   07   06   05   04   03   02   01   00
        // --------------------------------------------------------------------
        // | REG   | LW | SCALE   | 1  | BS | IS | BD SIZE  | 0  | IIS        |
        // --------------------------------------------------------------------

        u16  reg   = xxxx____________ (ea.ext1);
        u16  lw    = ____x___________ (ea.ext1);
        u16  scale = _____xx_________ (ea.ext1);
        u16  bs    = ________x_______ (ea.ext1);
        u16  is    = _________x______ (ea.ext1);
        u16  size  = __________xx____ (ea.ext1);
        u16  iis   = _____________xxx (ea.ext1);
        u32  base  = ea.ext2;
        u32  outer = ea.ext3;

        auto baseDisplacement = [&]() {
            size == 3 ? (*this << Int{(i32)base}) : (*this << Int{(i16)base});
        };
        auto baseRegister = [&]() {
            if constexpr (M == Mode(10)) {
                bs ? *this << Zpc{} : *this << Pc{};
            } else {
                if (!bs) *this << An{ea.reg};
            }
        };
        auto indexRegister = [&]() {
            if (!is) *this << Sep{} << Rn{reg} << (lw ? ":l" : ":w") << Scale{scale};
        };
        auto outerDisplacement = [&]() {
            *this << Int(outer);
        };

        if (iis == 0) {

            // Memory Indirect Unindexed
            baseRegister();
            *this << "@(";
            baseDisplacement(); indexRegister();
            *this << ")";

        } else if (iis & 0b100) {

            // Memory Indirect Postindexed
            baseRegister();
            *this << "@(";
            baseDisplacement();
            *this << ")@(";
            outerDisplacement(); indexRegister();
            *this << ")";

        } else {

            // Memory Indirect Preindexed
            baseRegister();
            *this << "@(";
            baseDisplacement(); indexRegister();
            *this << ")@(";
            outerDisplacement();
            *this << ")";
        }
    }

    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(IxMus<M, S> wrapper)
{
    assert(M == Mode(6) || M == Mode(10));

    auto &ea = wrapper.ea;

    u16 full = _______x________ (ea.ext1);

    if (!full) {

        //   15-12   11   10   09   08   07   06   05   04   03   02   01   00
        // --------------------------------------------------------------------
        // | REG   | LW | SCALE   | 0  | DISPLACEMENT                         |
        // --------------------------------------------------------------------

        u16 reg   = xxxx____________ (ea.ext1);
        u16 lw    = ____x___________ (ea.ext1);
        u16 scale = _____xx_________ (ea.ext1);
        u16 disp  = ________xxxxxxxx (ea.ext1);

        *this << "(";
        if (disp) *this << Int{(i8)disp} << ",";
        M == Mode(10) ? *this << Pc{} : *this << An{ea.reg};
        *this << "," << Rn{reg};
        *this << (lw ? ".l" : ".w");
        *this << Scale{scale} << ")";

    } else {

        //   15-12   11   10   09   08   07   06   05   04   03   02   01   00
        // --------------------------------------------------------------------
        // | REG   | LW | SCALE   | 1  | BS | IS | BD SIZE  | 0  | IIS        |
        // --------------------------------------------------------------------

        u16  reg   = xxxx____________ (ea.ext1);
        u16  lw    = ____x___________ (ea.ext1);
        u16  scale = _____xx_________ (ea.ext1);
        u16  bs    = ________x_______ (ea.ext1);
        u16  is    = _________x______ (ea.ext1);
        u16  size  = __________xx____ (ea.ext1);
        u16  iis   = _____________xxx (ea.ext1);
        u32  base  = ea.ext2;
        u32  outer = ea.ext3;

        bool preindex = (iis > 0 && iis < 4);
        bool postindex = (iis > 4);
        bool effectiveZero = (ea.ext1 & 0xe4) == 0xC4 || (ea.ext1 & 0xe2) == 0xC0;
        bool comma = false;

        if (effectiveZero) {

            *this << "0";
            return *this;
        }

        *this << "(";

        if (preindex || postindex) {

            *this << "[";
        }
        if (base) {

            size == 3 ? (*this << Int{(i32)base}) : (*this << Int{(i16)base});
            comma = true;
        }
        if (!bs) {

            if (comma) *this << ",";
            M == Mode(10) ? *this << Pc{} : *this << An{ea.reg};
            comma = true;
        }
        if (postindex) {

            *this << "]";
            comma = true;
        }
        if (!is) {

            if (comma) *this << ",";
            *this << Rn{reg};
            *this << (lw ? ".l" : ".w");
            *this << Scale{scale};
            comma = true;
        }
        if (preindex) {

            *this << "]";
            comma = true;
        }
        if(outer)
        {
            if (comma) *this << ",";
            *this << Int(outer);
        }

        *this << ")";
    }

    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(Aw<M, S> wrapper)
{
    auto &ea = wrapper.ea;
    *this << UInt(ea.ext1) << Sz<Word>{};

    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(Al<M, S> wrapper)
{
    auto &ea = wrapper.ea;
    *this << UInt(ea.ext1) << Sz<Long>{};
    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(DiPc<M, S> wrapper)
{
    auto &ea = wrapper.ea;
    u32 resolved;

    *this << "(" << Int{(i16)ea.ext1} << ",PC)";
    resolved = U32_ADD(U32_ADD(ea.pc, (i16)ea.ext1), 2);
    StrWriter(comment, style) << "; (" << UInt(resolved) << ")" << Finish{};

    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(Im<M, S> wrapper)
{
    auto &ea = wrapper.ea;

    *this << Imu<S>(ea.ext1);

    return *this;
}

template <Mode M, Size S> StrWriter&
StrWriter::operator<<(Ip<M, S> wrapper)
{
    auto &ea = wrapper.ea;
    *this << "-(" << An{ea.reg} << ")";
    return *this;
}

StrWriter&
StrWriter::operator<<(Scale s)
{
    if (!s.raw) return *this;

    *ptr++ = '*';
    *ptr++ = '0' + (char)(1 << s.raw);

    return *this;
}

StrWriter&
StrWriter::operator<<(Fc fc)
{
    // 10XXX — Function code is specified as bits XXX
    // 01DDD — Function code is specified as bits 2–0 of data register DDD
    // 00000 — Function code is specified as source function code register
    // 00001 — Function code is specified as destination function code register

    switch (fc.raw >> 3 & 0b11) {

        case 0b10:  *this << Imu(fc.raw & 0b111); break;
        case 0b01:  *this << Dn(fc.raw & 0b111); break;

        case 0b00:

            if (fc.raw == 0) { *this << Sfc(); break; }
            if (fc.raw == 1) { *this << Dfc(); break; }
            [[fallthrough]];

        default:    *this << Imu(fc.raw & 0b1111);
    }

    return *this;
}

StrWriter&
StrWriter::operator<<(Sfc _)
{
    *ptr++ = 'S'; *ptr++ = 'F'; *ptr++ = 'C';

    return *this;
}

StrWriter&
StrWriter::operator<<(Dfc _)
{
    *ptr++ = 'D'; *ptr++ = 'F'; *ptr++ = 'C';

    return *this;
}

StrWriter&
StrWriter::operator<<(Fp fp)
{
    *ptr++ = 'F'; *ptr++ = 'P'; *ptr++ = '0' + (char)fp.raw;
    return *this;
}

StrWriter&
StrWriter::operator<<(Ffmt ffmt)
{
    *ptr++ = '.';

    switch (ffmt.raw) {

        case 0: *ptr++ = 'l'; break;
        case 1: *ptr++ = 's'; break;
        case 2: *ptr++ = 'x'; break;
        case 3: *ptr++ = 'p'; break;
        case 4: *ptr++ = 'w'; break;
        case 5: *ptr++ = 'd'; break;
        case 6: *ptr++ = 'b'; break;

        default:
            *ptr++ = '?';
    }

    return *this;
}

StrWriter&
StrWriter::operator<<(Fctrl fctrl)
{
    const char *prefix = "";
    const char *delim = "";

    if (fctrl.raw == 0) { *this << "{}"; }
    if (fctrl.raw & 1) { *this << delim << prefix << "fpiar"; delim = "/"; }
    if (fctrl.raw & 2) { *this << delim << prefix << "fpsr";  delim = "/"; }
    if (fctrl.raw & 4) { *this << delim << prefix << "fpcr";  delim = "/"; }

    return *this;
}

StrWriter&
StrWriter::operator<<(Tab tab)
{
    do { *ptr++ = ' '; } while (ptr < base + tab.raw);
    return *this;
}

template <Instr I, Mode M, Size S> StrWriter&
StrWriter::operator<<(const Av<I, M, S> &av)
{
    switch (I) {

        case Instr::BKPT:
        case Instr::MOVES:
        case Instr::MOVEFCCR:
        case Instr::RTD:

            *this << "; (1+)";
            break;

        case Instr::CMPI:

            *this << (isPrgMode(M) ? "; (1+)" : "");
            break;

        case Instr::CALLM:
        case Instr::RTM:

            *this << "; (2)";
            break;

        case Instr::cpGEN:
        case Instr::cpRESTORE:
        case Instr::cpSAVE:
        case Instr::cpScc:
        case Instr::cpTRAPcc:

            *this << "; (2-3)";
            break;

        case Instr::BFCHG:
        case Instr::BFCLR:
        case Instr::BFEXTS:
        case Instr::BFEXTU:
        case Instr::BFFFO:
        case Instr::BFINS:
        case Instr::BFSET:
        case Instr::BFTST:
        case Instr::CAS:
        case Instr::CAS2:
        case Instr::CHK2:
        case Instr::CMP2:
        case Instr::DIVL:
        case Instr::EXTB:
        case Instr::MULL:
        case Instr::PACK:
        case Instr::TRAPCC:
        case Instr::TRAPCS:
        case Instr::TRAPEQ:
        case Instr::TRAPGE:
        case Instr::TRAPGT:
        case Instr::TRAPHI:
        case Instr::TRAPLE:
        case Instr::TRAPLS:
        case Instr::TRAPLT:
        case Instr::TRAPMI:
        case Instr::TRAPNE:
        case Instr::TRAPPL:
        case Instr::TRAPVC:
        case Instr::TRAPVS:
        case Instr::TRAPF:
        case Instr::TRAPT:
        case Instr::UNPK:

            *this << "; (2+)";
            break;

        case Instr::CHK:
        case Instr::LINK:
        case Instr::BRA:
        case Instr::BHI:
        case Instr::BLS:
        case Instr::BCC:
        case Instr::BCS:
        case Instr::BNE:
        case Instr::BEQ:
        case Instr::BVC:
        case Instr::BVS:
        case Instr::BPL:
        case Instr::BMI:
        case Instr::BGE:
        case Instr::BLT:
        case Instr::BGT:
        case Instr::BLE:
        case Instr::BSR:

            *this << (S == Long ? "; (2+)" : "");
            break;

        case Instr::TST:
            *this << (M == Mode(1) || M >= Mode(9) ? "; (2+)" : "");
            break;

        case Instr::CINV:
        case Instr::CPUSH:
        case Instr::MOVE16:

            *this << "; (4)";
            break;

        case Instr::MOVEC:

            switch (av.ext1 & 0x0FFF) {

                case 0x000:
                case 0x001:
                case 0x800:
                case 0x801: *this << "; (1+)"; break;
                case 0x002:
                case 0x803:
                case 0x804: *this << "; (2+)"; break;
                case 0x802: *this << "; (2,3)"; break;
                case 0x003:
                case 0x004:
                case 0x005:
                case 0x006:
                case 0x007:
                case 0x805:
                case 0x806:
                case 0x807: *this << "; (4+)"; break;

                default:    *this << "; (?)";
            }

        default:
            break;
    }
    return *this;
}

StrWriter&
StrWriter::operator<<(Sep)
{
    *ptr++ = ',';
    return *this;
}

StrWriter&
StrWriter::operator<<(Finish)
{
    // Append comment
    for (int i = 0; comment[i] != 0; i++) *ptr++ = comment[i];

    // Terminate the string
    *ptr = 0;

    return *this;
}
