/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    disk_image.hpp: A disk image, and the three things a controller might be allowed to do to one.

    Every storage controller used to hold a FileStream* and seek/read/write it directly, which meant
    the emulated machine wrote to the real image on every guest write and there was no way to say
    "don't". That is a problem for development rather than for correctness: IRIX's filesystem only
    flushes its free bitmap from efs_update, so a machine killed without a sync leaves an inode
    pointing at a block the on-disk bitmap still calls free, and the next boot prints
    "filesystem corruption on md0a". A debugging session that ends in a kill - which most of them do -
    therefore damages the disk a little every time, and the damage looks exactly like an emulator bug.

    So there are three modes:

        Direct      straight through to the file, exactly as it always was
        Overlay     copy on write - the base is opened READ ONLY and dirty sectors are held in memory
        ReadOnly    writes are refused outright

    Overlay is the useful one. The base image cannot be touched however the emulator dies, two
    emulators on one image stop being dangerous because neither has it open for writing, and the
    per-sector flush() that the controllers do on every write disappears. What it costs is that guest
    writes are thrown away when the process exits - deliberately - unless you ask for them:
    Commit() folds the overlay back into the base, and +set diskCommitOnExit 1 does that on shutdown,
    which is how you keep something you actually wanted (an /etc/fsck repair, say).
*/

#pragma once
#include <Motion.hpp>
#include <base/filesystem/filesystem.hpp>

namespace Motion
{
    // Every SGI disk of this era is 512 byte sectors and the label says so, so the overlay is
    // granular to that. Nothing here assumes the guest transfers a whole sector at a time.
    #define DISK_IMAGE_SECTOR_SIZE          512

    #define DISK_IMAGE_LOG_PREFIX           "Disk"

    enum class DiskWriteMode
    {
        /// @brief Guest writes go straight to the image file. What the emulator did before there was a choice.
        Direct,

        /// @brief Copy on write. The base is opened read only and every written sector is kept in memory.
        Overlay,

        /// @brief Writes are refused and reported to the guest as an error.
        ReadOnly,
    };

    class DiskImage
    {
    public:
        /// @brief Open an image. Returns nullptr if the file is not there or cannot be opened for the
        ///        mode asked for. The path is used as given - Profile::OpenDisk resolves it first.
        static DiskImage* Open(const char* path, DiskWriteMode mode);

        /// @brief Turn a cvar string into a mode. Anything unrecognised warns and gives Direct.
        static DiskWriteMode ModeFromString(const char* name);
        static const char* ModeName(DiskWriteMode mode);

        ~DiskImage();

        /// @brief Read `length` bytes at `offset`. False if any of it lay outside the image, in which
        ///        case nothing is guaranteed about the buffer.
        bool Read(size_t offset, uint8_t* buffer, size_t length);

        /// @brief Write `length` bytes at `offset`. False if it lay outside the image, or if the mode
        ///        does not permit writing.
        bool Write(size_t offset, const uint8_t* buffer, size_t length);

        size_t GetSize() const { return size; };
        DiskWriteMode GetMode() const { return mode; };
        const char* GetPath() const { return path.c_str(); };

        /// @brief How many sectors the overlay is holding. Always 0 outside Overlay mode.
        size_t GetDirtySectorCount() const { return overlay.size(); };

        /// @brief Fold the overlay into the base image, permanently. Reopens the file read-write to
        ///        do it, so it works even though Overlay mode opened it read only.
        bool Commit();

        /// @brief Throw the overlay away, putting the disk back to the state the file is in.
        void Discard();

    private:
        DiskImage() = default;

        bool ReadBase(size_t offset, uint8_t* buffer, size_t length);

        /// @brief Get the overlay's copy of a sector, reading the base into it first if this is the
        ///        first time it has been written. Returns nullptr if the sector is out of range.
        uint8_t* MaterialiseSector(size_t sector);

        std::string path;
        DiskWriteMode mode = DiskWriteMode::Direct;
        FileStream* stream = nullptr;
        size_t size = 0;

        // Sector index -> its contents. Sparse on purpose: a boot dirties a few hundred sectors, and
        // the worst case is the whole image, which for these disks is 60MB.
        std::unordered_map<size_t, std::vector<uint8_t>> overlay;

        // The "this image is read only" complaint is worth making once, not once per sector.
        bool refusalLogged = false;
    };
};
