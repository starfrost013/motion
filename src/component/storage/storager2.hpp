/* motion - The SGI Emulator. Copyright (c)2026 danifunker. storager2.hpp: The Interphase Storager 2 Multibus ESDI disk controller - SGI's "sii", what makes this a 3130 */

#pragma once
#include <Motion.hpp>
#include <component/component.hpp>
#include <component/multibus/multibus.hpp>
#include <base/profile/profile.hpp>
#include <base/filesystem/filesystem.hpp>

namespace Motion
{
    // 68020 addresses. Stops at 0x73FB on purpose: R4-R7 *are* siq0, and siqprobe() returns CONF_ALIVE for anything that answers there.
    #define STORAGER2_MBIO_START                0x50007200
    #define STORAGER2_MBIO_END                  0x500073FB

    // The board's own 512 byte window. Indexed in Multibus byte offsets throughout.
    #define STORAGER2_MBIO_SIZE                 0x200
    #define STORAGER2_MBIO_MASK                 0x1FF

    // R0-R7 land in order at the top of the block once the byte lanes are uncrossed.
    #define STORAGER2_REG_R0                    0x1F8
    #define STORAGER2_NUM_REGS                  8

    // The control word block. One word per IOPB, then two registers of housekeeping.
    #define STORAGER2_NUM_IOPBS                 14      // NUMBERIOPBS in siireg.h
    #define STORAGER2_CW_STRIDE                 2
    #define STORAGER2_OFF_OVERLAP               0x1C    // overlapped seek enable, written and ignored
    #define STORAGER2_OFF_NUMIOPBS              0x1E    // how many IOPBs the host is using
    #define STORAGER2_OFF_STARTIOPB             0x1F    // which one the queue starts at

    // The IOPB ring. siistart() addresses IOPB n at SC_IOPBBASE + (n + 1) * 0x20.
    #define STORAGER2_IOPB_BASE                 0x20
    #define STORAGER2_IOPB_STRIDE               0x20

    // Bits written to R0 (siireg.h, "Macros for Writing to ST_R0")
    #define STORAGER2_CMD_START                 0x01
    #define STORAGER2_CMD_CLEAR                 0x02
    #define STORAGER2_CMD_NOINTERRUPT           0x10
    #define STORAGER2_CMD_16BITS                0x20
    #define STORAGER2_CMD_ABORT                 0x40
    #define STORAGER2_CMD_RESET                 0x80

    // Bits read back from R0 (siireg.h, "Macros for Reading from ST_R0")
    #define STORAGER2_ST_BUSY                   0x01
    #define STORAGER2_ST_DONE                   0x02
    #define STORAGER2_ST_STCHANGE               0x04
    #define STORAGER2_ST_ERROR                  0x08
    #define STORAGER2_ST_QUEUEMODE              0x80

    // Bits in a control word. The host writes SC_ENABLE (0x83) to launch that IOPB.
    #define STORAGER2_SC_ENINTERRUPT            0x01
    #define STORAGER2_SC_INTBON                 0x02
    #define STORAGER2_SC_ENIOPB                 0x80

    // The IOPB in Multibus byte offsets, spelled out byte by byte in stand/include/stdreg.h's comments.
    #define STORAGER2_IOPB_OFF_COMMAND          0x00
    #define STORAGER2_IOPB_OFF_OPTIONS          0x01
    #define STORAGER2_IOPB_OFF_STATUS           0x02
    #define STORAGER2_IOPB_OFF_ERROR            0x03
    #define STORAGER2_IOPB_OFF_UNIT             0x04
    #define STORAGER2_IOPB_OFF_HEAD             0x05
    #define STORAGER2_IOPB_OFF_CYLINDER         0x06
    #define STORAGER2_IOPB_OFF_SECTOR           0x08
    #define STORAGER2_IOPB_OFF_COUNT            0x0A
    #define STORAGER2_IOPB_OFF_BURST            0x0C
    #define STORAGER2_IOPB_OFF_BUFFER           0x0D

    // What C_REPORT answers with. Nothing reads it but a debug print; stdprobe() only needs the command to succeed.
    #define STORAGER2_REPORT_FIRMWARE           0x21    // "rev 2.1", read as two nibbles
    #define STORAGER2_REPORT_EXTENSION          0x00    // release letter, 0 for none
    #define STORAGER2_REPORT_PRODUCT            0x02    // product code - a guess, nothing reads it
    #define STORAGER2_REPORT_OPTIONS            0x0000  // zero suppresses sifex's "Options:" line

    // Status byte the controller posts back into the IOPB. siicmd() polls this and nothing else.
    #define STORAGER2_S_OK                      0x80
    #define STORAGER2_S_BUSY                    0x81
    #define STORAGER2_S_ERROR                   0x82

    // Commands. The full list is in siilist.h; these are the ones the driver ever issues.
    #define STORAGER2_C_READ                    0x81
    #define STORAGER2_C_WRITE                   0x82
    #define STORAGER2_C_VERIFY                  0x83
    #define STORAGER2_C_FORMAT                  0x84
    #define STORAGER2_C_MAP                     0x85
    #define STORAGER2_C_REPORT                  0x86
    #define STORAGER2_C_INIT                    0x87
    #define STORAGER2_C_RESTORE                 0x89
    #define STORAGER2_C_SEEK                    0x8A
    #define STORAGER2_C_REFORMAT                0x8B
    #define STORAGER2_C_RESET                   0x8F
    #define STORAGER2_C_RDABSOLUTE              0x93
    #define STORAGER2_C_READNOCACHE             0x94

    // From sii_errs[] in siilist.h - the driver prints these through dkerror(), so the right one says what went wrong.
    #define STORAGER2_ERR_NOTREADY              0x10    // disk not ready
    #define STORAGER2_ERR_BADUNIT               0x11    // invalid disk unit address
    #define STORAGER2_ERR_BADCOMMAND            0x14    // invalid command code
    #define STORAGER2_ERR_BADCYLINDER           0x15    // invalid cylinder address in iopb
    #define STORAGER2_ERR_BADSECTOR             0x16    // invalid sector number in iopb
    #define STORAGER2_ERR_BUSTIMEOUT            0x18    // bus timeout error
    #define STORAGER2_ERR_WRITEPROTECT          0x1A    // disk write protected
    #define STORAGER2_ERR_SECTORNOTFOUND        0x29    // sector not found
    #define STORAGER2_ERR_NOTINITIALISED        0x40    // unit not initialized
    #define STORAGER2_ERR_BADSPT                0x50    // sectors per track specification error
    #define STORAGER2_ERR_BADSECTORSIZE         0x51    // bytes per sector specification error
    #define STORAGER2_ERR_BADHEAD               0x53    // invalid head number in iopb
    #define STORAGER2_ERR_BADCYLCOUNT           0x61    // maximum cylinder number specification error
    #define STORAGER2_ERR_BADHEADCOUNT          0x62    // number of heads specification error

    // The 32 byte UIB, Multibus offsets. Little endian where the IOPB is big endian - that is the board, not a mistake.
    #define STORAGER2_UIB_SIZE                  0x20
    #define STORAGER2_UIB_OFF_HEADS             0x00
    #define STORAGER2_UIB_OFF_SPT               0x01
    #define STORAGER2_UIB_OFF_BPS_LOW           0x02
    #define STORAGER2_UIB_OFF_BPS_HIGH          0x03
    #define STORAGER2_UIB_OFF_DDB               0x12    // drive descriptor byte - ESDI vs ST506
    #define STORAGER2_UIB_OFF_CYL_LOW           0x1A
    #define STORAGER2_UIB_OFF_CYL_HIGH          0x1B

    // Units 0 and 1 are winchesters, unit 2 is the floppy (FLP_OFFSET in siireg.h).
    #define STORAGER2_MAX_WINCHESTERS           2
    #define STORAGER2_FLOPPY_UNIT               2
    #define STORAGER2_MAX_UNITS                 3

    // The only sector size this understands, and the only one SGI ever formats a drive with.
    #define STORAGER2_BLOCK_SIZE                512

    // Multibus 5 -> vector 0x45 -> level5() -> siiintr(). Only ever driven for a completion the driver will collect: a stray level 5 panics.
    #define STORAGER2_MULTIBUS_IRQ_LEVEL        5

    #define STORAGER2_LOG_PREFIX                "Storager 2"

    // Nothing arbitrates slot numbers; the DSD picked 7 out of the air, so this one is 8.
    #define STORAGER2_MULTIBUS_SLOTNUM          8

    // How many status reads the reset self test appears to take. See ResetController().
    #define STORAGER2_RESET_SETTLE_READS        4

    extern Cvar* logStorager;

    class Storager2 : public Component
    {
    public:
        void Start() override;
        void Shutdown() override;

        const char* GetName() override { return "Interphase Storager 2"; };

        uint8_t Read8(size_t addr) override;
        uint16_t Read16(size_t addr) override;
        uint32_t Read32(size_t addr) override;
        void Write8(size_t addr, uint8_t value) override;
        void Write16(size_t addr, uint16_t value) override;
        void Write32(size_t addr, uint32_t value) override;

    private:
        /// @brief One IOPB, decoded out of whichever memory it happened to live in.
        struct IOPB
        {
            uint8_t command;
            uint8_t options;
            uint8_t unit;
            uint8_t head;
            uint16_t cylinder;
            uint16_t sector;
            uint16_t sectorCount;
            uint32_t bufferAddress;     // 24 bit Multibus address
        };

        /// @brief Per drive state. A drive is only usable once a C_INIT has given it a geometry.
        struct Drive
        {
            DiskImage* image = nullptr;
            size_t imageSize = 0;
            bool initialised = false;
            uint16_t cylinders = 0;
            uint16_t bytesPerSector = 0;
            uint8_t heads = 0;
            uint8_t sectorsPerTrack = 0;
        };

        /// @brief A finished queued IOPB waiting for the driver to come and read it out of R0/R1.
        struct Completion
        {
            uint8_t iopb;
            uint8_t unit;
            bool error;
        };

        Multibus* multibus = nullptr;

        // The board's 512 bytes in Multibus order. The IOPB ring is real RAM: siistart() stashes a struct buf * in each.
        uint8_t io[STORAGER2_MBIO_SIZE] = {0};

        Drive drives[STORAGER2_MAX_UNITS];

        // The IOPB's Multibus address, from R1/R2/R3. All ones is siistart() asking for queued mode.
        uint32_t iopbAddress = 0;

        bool queueMode = false;

        // The non-queued half of DONE/ERROR; queued completions carry their own, since several can be outstanding.
        bool done = false;
        bool error = false;

        // The reset self test, counted in status reads rather than time. See ReadStatusRegister().
        int32_t resetSettleReads = 0;

        Completion completions[STORAGER2_NUM_IOPBS + 2] = {0};
        int32_t completionHead = 0;
        int32_t completionCount = 0;

        bool irqAsserted = false;
        bool logEnabled = false;

        // Multibus byte N is the byte the 68020 wrote at N ^ 1.
        uint8_t MBRead8(size_t mbAddr) { return multibus->ReadMB8(mbAddr ^ 1); };
        void MBWrite8(size_t mbAddr, uint8_t value) { multibus->WriteMB8(mbAddr ^ 1, value); };

        // Register file access, in Multibus offsets.
        uint8_t ReadRegister(size_t mbOffset);
        void WriteRegister(size_t mbOffset, uint8_t value);

        uint8_t ReadStatusRegister();
        void WriteCommandRegister(uint8_t value);
        void ResetController();
        void ClearCompletion();

        // An IOPB lives either in Multibus memory or in our own window, so every path has to work both ways.
        uint8_t IOPBRead8(bool queued, size_t base, size_t offset);
        void IOPBWrite8(bool queued, size_t base, size_t offset, uint8_t value);
        void FetchIOPB(bool queued, size_t base, IOPB& out);

        /// @brief Run one IOPB and post its status back. Returns true if it succeeded.
        bool ExecuteIOPB(bool queued, size_t base);

        bool Initialise(const IOPB& iopb, uint8_t& errorOut);
        bool Transfer(const IOPB& iopb, bool write, uint8_t& errorOut);
        bool CheckAddress(const IOPB& iopb, const Drive& drive, size_t& linearOut, uint8_t& errorOut);

        void AssertIRQLine();
        void ClearIRQLine();

        void Trace(const std::string& message);
    };
};
