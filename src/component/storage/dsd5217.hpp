/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    dsd5217.hpp: The Qualogy (previously known as Data Systems Design) DSD 5217 Multibus Disk & Tape Controller
    This is a combined QIC tape, hard drive and floppy controller.
    
    Technically not used on the 3130 (3120) but this is the only controller that I've got a disk image for right now
    Later on we can run mkboot and boot this

    Currently this is a high-level emulation, but this uses the Intel 8085. Later on we'll write an 8085 emulation.

    The controller is a Multibus BUS MASTER and decodes NOTHING except its single programmed I/O port.
    The wake-up block, channel control block, controller invocation block, I/O parameter block and every
    data buffer live in ordinary Multibus RAM; the controller walks that chain of pointers itself when the
    host pokes a start command into the port. We do the same, rather than trying to snoop the host's writes
    to those structures - claiming a memory window for them punches a hole in whatever the host is DMAing
    through, which is exactly the sort of thing that eats half a kernel.

    Sources:
    https://bitsavers.trailing-edge.com/pdf/dsd/5215_5217/040040-01_5215_Users_Guide_198404.pdf
    https://bitsavers.trailing-edge.com/pdf/dsd/5215_5217/040069-01_5217_Users_Guide_Addendu_198404.pdf
*/

#pragma once
#include <Motion.hpp>
#include <component/component.hpp>
#include <component/multibus/multibus.hpp>
#include <base/profile/profile.hpp>
#include <base/filesystem/filesystem.hpp>

namespace Motion
{
    // The controller answers on a single jumper-selected programmed I/O address. Only writes are decoded.
    /*
        "Only I/O write operations are recognized" - and only at one address. The board decodes a
        single byte wide programmed I/O port, which the console reports as "dsd0 at mbio 0x7f00".

        This used to claim the whole 0x7F00-0x7FFF page. Nothing else answers in there, so every read
        of it came back as 0xFF from this board, and the kernel probing for an EXOS Ethernet at
        0x7ffc read that, decided a board was present, attached it and then sat in _exconfig waiting
        forever for a controller that is not fitted.
    */
    #define DSD5217_MBIO_START                  0x50007F00
    #define DSD5217_MBIO_END                    0x50007F01
    #define DSD5217_MBIO_COMMAND                0x7F01 // all addresses are 1mb region

    // Programmed I/O commands. Only the two least significant bits of the written byte are decoded.
    // (5215 User Guide, 4.6.1 Input/Output Commands)
    #define DSD5217_IO_COMMAND_MASK             0x03
    #define DSD5217_IO_CLEAR                    0x00    // clear interrupt / remove reset
    #define DSD5217_IO_START                    0x01    // start operation
    #define DSD5217_IO_RESET                    0x02    // reset controller

    // 20 bit seg:off addressing or 24 bit linear. we only implement 24 bit linear
    #define DSD5217_24BIT_ADDRESSING            7

    // The wake-up block lives at a jumper-selected Multibus memory address. SGI hardcode this one.
    // It is READ by the controller out of Multibus RAM - we do not decode it.
    #define DSD5217_WUB_ADDRESS                 0x7F000
    #define DSD5217_WUB_OFF_EXTENSION           0x00    // multibus byte offsets within the WUB
    #define DSD5217_WUB_OFF_CCB_PTR             0x02

    /*
        Control block pointers are paragraph (16 byte) granular: the low four bits of every block pointer
        the controller chains through are ignored. SGI's PROM depends on this - it hands the controller a
        CIB pointer four bytes into its own structure and expects it to be rounded back down, which is the
        only way its operation status byte, status semaphore and IOPB pointer land where the manual says
        they should. Data buffer addresses are NOT rounded.
    */
    // Block pointers are 24 bit Multibus addresses and are used as they stand.
    #define DSD5217_BLOCK_PTR_MASK              0xFFFFFF

    /*
        The CIB pointer is the one exception: it names the CIB's byte 4 rather than its base. SGI's
        driver builds the block, hands the controller `lea 4(cib)`, and then writes operation status,
        the semaphores and the IOPB pointer at the manual's offsets from the *base*.

        This used to be handled by rounding every block pointer down to a 16 byte boundary, which is
        the same thing only while the block happens to be paragraph aligned. The PROM's blocks are;
        the kernel's are not - its CCB sits at multibus 0x1cde and its CIB at 0x1cee - so rounding
        read every field fourteen bytes low, the CIB pointer came back as zero and dsdinit sat in its
        ten million iteration timeout and printed "dsd0: ccb timeout during init".
    */
    #define DSD5217_CIB_PTR_BIAS                4

    // this is configurable on the real thing with jumpers but for now just do this
    #define DSD5217_MULTIBUS_IRQ_LEVEL          1

    #define DSD5217_LOG_PREFIX                  "DSD 5217"

    // Fresh from my ass
    #define DSD5217_MULTIBUS_SLOTNUM            7

    // Device codes. These determine which device we are actually communicating with
    #define DSD5217_DEVICE_CODE_HDD             0   // 'Winchester' (Hard disk drive)
    #define DSD5217_DEVICE_CODE_FLOPPY          1   // Floppy Disk
    #define DSD5217_DEVICE_CODE_QIC             2   // QIC tape drive
    #define DSD5217_DEVICE_CODE_TAPE            3   // Tape
    #define DSD5217_DEVICE_CODE_217_TAPE        4   // iSBX 217 emulation tape functions (5217 addendum)

    // Number of each
    #define DSD5217_MAX_DISK_DRIVES             2   
    #define DSD5217_MAX_QIC_DRIVES              2   
    #define DSD5217_MAX_FLOPPY_DRIVES           2   

    // block size
    #define DSD5217_BLOCK_SIZE                  512

    // operation status
    #define DSD5217_OPERATION_STATUS_COMPLETE   (1 << 0)
    #define DSD5217_OPERATION_SEEK_COMPLETE     (1 << 1)
    #define DSD5217_OPERATION_MEDIA_CHANGE      (1 << 2)
    #define DSD5217_OPERATION_FLOPPYQIC_DONE    0x09
    #define DSD5217_OPERATION_TAPE_MEDIA_CHANGE 0x0E
    #define DSD5217_OPERATION_TAPE_LONG_COMMAND 0x0F
    #define DSD5217_OPERATION_UNIT_BITS         0x30        // these determine which unit initiated the operaiton
    #define DSD5217_OPERATION_UNIT_SHIFT        4
    #define DSD5217_OPERATION_HARD_ERROR        (1 << 6)    // ah crap
    #define DSD5217_OPERATION_SUMMARY_ERROR     (1 << 7)
    #define DSD5217_OPERATION_STATUS_MASK       0xF0

    // function codes
    // there are 32
    #define DSD5217_FUNC_INIT                   0x00        // start
    #define DSD5217_FUNC_XFER_STATUS            0x01        // how am i doing
    #define DSD5217_FUNC_FORMAT                 0x02        // the death of your data
    #define DSD5217_FUNC_READ_ID                0x03        // read the id of something
    #define DSD5217_FUNC_READ_DATA              0x04        // read something
    #define DSD5217_FUNC_READ_AND_VERIFY        0x05        // read to buffer and verify
    #define DSD5217_FUNC_WRITE                  0x06        // write, the birth of your data
    #define DSD5217_FUNC_WRITE_BUFFER           0x07        // write into a buffer
    #define DSD5217_FUNC_SEEK                   0x08        // go somewhere
    // 0x09-0x0d are reserved
    #define DSD5217_FUNC_BUFFER_IO              0x0E        // generic buffer i/o
    #define DSD5217_FUNC_DIAG                   0x0F        // figure out if it really is over
    #define DSD5217_FUNC_TAPE_INIT              0x10        // initialise tape
    #define DSD5217_FUNC_TAPE_REWIND            0x11        // rewind tape
    #define DSD5217_FUNC_TAPE_FORWARD_FILE      0x12        // go forward 1 file
    #define DSD5217_FUNC_TAPE_WRITE_MARK        0x14        // write a file mark
    #define DSD5217_FUNC_TAPE_ERASE             0x17        // no more data
    #define DSD5217_FUNC_TAPE_FORWARD_RECORD    0x1A        // go forward 1 record
    #define DSD5217_FUNC_TAPE_RESET             0x1C        // restart
    #define DSD5217_FUNC_TAPE_RETENSION         0x1D        // retension
    #define DSD5217_FUNC_TAPE_DRIVE_STATUS      0x1E        // how are you doing, tape drive?
    #define DSD5217_FUNC_TAPE_RW_TERMINATE      0x1F        // you're done for today tape

    // operation modifiers
    #define DSD5217_MODIFIER_NO_INT             (1 << 0)    // no interrupt on completion
    #define DSD5217_MODIFIER_NO_RETRY           (1 << 1)    // don't retry for error recovery
    #define DSD5217_MODIFIER_ALLOW_DELETED      (1 << 2)    // allow deleted data rewrite
    #define DSD5217_MODIFIER_READ_AFTER_LONG    (1 << 6)    // read tape after long operation

    // soft error
    #define DSD5217_SOFTERR_DATA_RECOVERABLE    (1 << 1)    // try again to read data, need a new tape
    #define DSD5217_SOFTERR_DATA_UNRECOVERABLE  (1 << 3)    // data is busted
    #define DSD5217_SOFTERR_DRIVE_FAULT         (1 << 5)    // drive is busted
    #define DSD5217_SOFTERR_BUFFER_OVERRUN      (1 << 7)    // buffer over/underrun

    // hard error (2 bytes)
    #define DSD5217_HARDERR0_REJECT_COMMAND5215 (1 << 0)    // invalid command in 5215 mode
    #define DSD5217_HARDERR0_REJECT_COMMAND5217 (1 << 1)    // invalid command in 5217 mode
    #define DSD5217_HARDERR0_INVALID_COMMAND    (1 << 2)    // invalid command
    #define DSD5217_HARDERR0_TAPEINIT_FAIL      (1 << 4)    // failed to initialise tape
    #define DSD5217_HARDERR0_LONGTERM_CMD       (1 << 5)    // Long term command
    #define DSD5217_HARDERR0_ILLEGAL_FORMAT     (1 << 6)    // illegal format
    #define DSD5217_HARDERR0_END_OF_MEDIA       (1 << 7)    // nothing left to read

    #define DSD5217_HARDERR1_LENGTH_ERROR       (1 << 0)    // media not multiple of block size long
    #define DSD5217_HARDERR1_TAPE_TIMEOUT       (1 << 2)    // tape timed out
    #define DSD5217_HARDERR1_INVALID_COMMAND    (1 << 3)    // check type in byte 0
    #define DSD5217_HARDERR1_NO_TAPE_CART       (1 << 4)    // bozo didn't put the tape in
    #define DSD5217_HARDERR1_INVALID_ADDRESS    (1 << 5)    // invalid address (something is wrong)
    #define DSD5217_HARDERR1_UNIT_NOT_READY     (1 << 6)    // unit not ready
    #define DSD5217_HARDERR1_WRITE_PROTECTED    (1 << 7)    // write protected
    
    #define DSD5217_MAXIMUM_BUFFER_SIZE         1024        // largest sector the on-board buffer holds
    
    // the coherent extension
    class CoherentExtensionDSD5217 : public CoherentExtension
    {
    public:
        CoherentExtensionDSD5217(Component* component) : CoherentExtension(component) { };

        void AddUI() override;
    };

    class DSD5217 : public Component
    {
        friend class CoherentExtensionDSD5217;
        
    public:
        DSD5217() : Component()
        {
        }

        void Start() override;
        void Shutdown() override; 

        const char* GetName() { return "DSD/Qualogy 5217 Multibus Disk & Tape Controller"; };

        /*
            These are DECODED COPIES of the control blocks, not overlays onto guest memory - the controller
            fetches them out of Multibus RAM when it is started. The comment on each field is its Multibus
            byte offset within the block, which is what the manuals use. Note that the IP2 crosses the byte
            lanes, so Multibus offset N is the byte the host wrote at N ^ 1: that is why the layout below
            looks transposed compared to what you see in a memory viewer.
        */

        /// @brief Wake Up Block (5215 User Guide, figure 4-4)
        struct WUB
        {
            uint8_t extension;              // +0: 7 = 24 bit linear addressing
            uint32_t ccbPtr;                // +2: CCB address
        }; 

        /// @brief Channel Control Block (5215 User Guide, figure 4-3)
        struct CCB
        {
            uint8_t ccw1;                   // +0: channel control word 1 (always 01h)
            uint8_t busy;                   // +1: ff = busy, 00 = idle
            uint32_t cibPtr;                // +2: CIB address
            uint8_t ccw2;                   // +8: not used (always 01h)
            uint8_t busy2;                  // +9: not used
            uint32_t cpPtr;                 // +a: not used
            uint16_t controlPtr;            // +e: not used (always 0004h)
        };

        /// @brief Controller Invocation Block (5217 Addendum figure 4-1, replaces 5215 section 4.6.7)
        struct CIB
        {
            uint8_t opStatus;               // +1: operation status
            uint8_t commandSemaphore;       // +2: the controller never touches this one
            uint8_t statusSemaphore;        // +3: status semaphore
            uint32_t iopbPtr;               // +8: IOPB Pointer
        };

        /// @brief Base of the CIB - see DSD5217_CIB_PTR_BIAS.
        size_t CIBAddress() { return (ccb.cibPtr & DSD5217_BLOCK_PTR_MASK) - DSD5217_CIB_PTR_BIAS; }

        /// @brief I/O Parameter Block (5215 User Guide, figure 4-3)
        struct IOPB
        {
            uint32_t actualTransfers;       // +4:  returned at end of operation
            uint16_t deviceCode;            // +8
            uint8_t unit;                   // +a
            uint8_t function;               // +b
            uint16_t modifier;              // +c
            uint16_t cylinder;              // +e
            uint8_t head;                   // +10
            uint8_t sector;                 // +11
            /* Data Buffer Address: This four-byte field contains the segmented address of the data buffer. 
            For normal read or write operations, this is the address of the multibus memory buffer where
            data is stored or fetched. For some commands, this is the address of additional control information */
            uint32_t dba;                   // +12
            uint32_t rbc;                   // +16: requested byte count
            uint32_t generalPtr;            // +1a: use as a pointer
        }; 

        /// @brief Initialisation Information Block - the data buffer used by the initialize command
        struct INIB
        {
            uint16_t nrCylinders;               // +0
            uint8_t fixedHeads;                 // +2
            uint8_t removableHeads;             // +3
            uint8_t sectorsPerTrack;            // +4
            uint8_t bytesPerSectorLow;          // +5
            uint8_t bytesPerSectorHigh;         // +6
            uint8_t numberOfAlternateCylinders; // +7
        };

        /// @brief format related/
        struct FMTB
        {
            uint8_t pattern1;
            uint8_t function;               // format function
            uint8_t pattern3;
            uint8_t pattern2;
            uint8_t interleave;
            uint8_t pattern4;
        }; 

        #define DSD5217_SB_SIZE                 14
    
        // Status bytes for operation?
        #define DSD5217_SB_HARD_ERROR0          0
        #define DSD5217_SB_HARD_ERROR1          1
        #define DSD5217_SB_SOFT_ERROR           2
        #define DSD5217_SB_BOT                  3
        #define DSD5217_SB_FILE_MARK            5
        #define DSD5217_SB_NO_DATA              8
        #define DSD5217_SB_RETRY_COUNT          11
        #define DSD5217_SB_DISK_EXTENDED_STATUS 12

        /// @brief sector descripton?
        struct INIST
        {
            DSD5217::INIB inib;
            DSD5217::FMTB fmtb;
            uint8_t sb[DSD5217_SB_SIZE];        // the status buffer ?
        }; 

        uint8_t state;                          // last byte written to the programmed i/o port

        // Methods
        uint8_t Read8(size_t addr) override;
        void Write8(size_t addr, uint8_t value) override;
        uint16_t Read16(size_t addr) override; 
        void Write16(size_t addr, uint16_t value) override;

    private: 
        // Multibus IRQ1 is used.
        Multibus* multibus;
        FileStream* hdd;
        CoherentExtensionDSD5217* dsdExtension;

        WUB wub = {0};
        CCB ccb = {0};
        CIB cib = {0};
        IOPB iopb = {0};
        INIST inist = {0};

        /* 
            Multibus <-> 68020 byte lane translation.

            The IP2 wires the 68020's D0-D7 to the Multibus' D8-D15 and vice versa, so the byte the
            controller sees at Multibus address N is the byte the CPU wrote to address N ^ 1. The 5215
            User Guide spells this out in the note under figure 4-3 ("The MC68000 looks at the bytes in
            reverse"). Anything wider than a byte is little endian from the controller's point of view -
            it is an 8085 - which is why the host stores every 32-bit field with its halves swapped.
        */
        uint8_t MBRead8(size_t mbAddr);
        uint16_t MBRead16(size_t mbAddr);
        uint32_t MBRead32(size_t mbAddr);
        void MBWrite8(size_t mbAddr, uint8_t value);
        void MBWrite16(size_t mbAddr, uint16_t value);
        void MBWrite32(size_t mbAddr, uint32_t value);

        // Chain through the control blocks in Multibus memory the way the real 8085 firmware does
        bool FetchWakeUpBlock();
        bool FetchChannelBlocks();
        void FetchIOPB();

        // Post the result of a command back into the CIB and drop the busy flag in the CCB
        void PostStatus(uint8_t opStatus);
        void SetControllerBusy(bool busy);

        // Methods related to causing the disk to actually do something
        size_t CHSToLinear();
        size_t GetBytesPerSector();

        bool ReadSector();
        bool ReadInitBlock();
        bool WriteStatusBlock();

        // can't do any disk ops if there is no disk inserted lmao
        bool diskIsOpen;

        // The controller comes up held in reset. The first start command after the reset is removed only
        // chains through the tables - it does not execute an IOPB. (5215 User Guide, 4.5 step C)
        bool inReset = true;
        bool tablesFetched = false;
        bool irqAsserted = false;

        // Only one sector can be read at a time
        uint8_t sectorBuffer[DSD5217_MAXIMUM_BUFFER_SIZE] = {0};

        // execute command
        void ExecuteCommand();
        void AssertIRQLine();
        void ClearIRQLine();
    }; 
}; 
