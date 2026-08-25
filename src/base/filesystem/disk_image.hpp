/* motion - The SGI Emulator. Copyright (c)2026 starfrost. disk_image.hpp: A disk image, written through (Direct), copy on write (Overlay) or not at all (ReadOnly) */

#pragma once
#include <Motion.hpp>
#include <base/filesystem/filesystem.hpp>

namespace Motion
{
    // Overlay granularity. Nothing here assumes the guest transfers a whole sector at a time.
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
        /// @brief Open an image, path as given. nullptr if it will not open for the mode asked for.
        static DiskImage* Open(const char* path, DiskWriteMode mode);

        /// @brief Turn a cvar string into a mode. Anything unrecognised warns and gives Direct.
        static DiskWriteMode ModeFromString(const char* name);
        static const char* ModeName(DiskWriteMode mode);

        ~DiskImage();

        /// @brief Read `length` bytes at `offset`. False, and an undefined buffer, if it ran outside the image.
        bool Read(size_t offset, uint8_t* buffer, size_t length);

        /// @brief Write `length` bytes at `offset`. False if outside the image, or if the mode forbids writing.
        bool Write(size_t offset, const uint8_t* buffer, size_t length);

        size_t GetSize() const { return size; };
        DiskWriteMode GetMode() const { return mode; };
        const char* GetPath() const { return path.c_str(); };

        /// @brief How many sectors the overlay is holding. Always 0 outside Overlay mode.
        size_t GetDirtySectorCount() const { return overlay.size(); };

        /// @brief Fold the overlay into the base, permanently. Reopens read-write, since Overlay opened it read only.
        bool Commit();

        /// @brief Throw the overlay away, putting the disk back to the state the file is in.
        void Discard();

    private:
        DiskImage() = default;

        bool ReadBase(size_t offset, uint8_t* buffer, size_t length);

        /// @brief The overlay's copy of a sector, seeded from the base on first write. nullptr if out of range.
        uint8_t* MaterialiseSector(size_t sector);

        std::string path;
        DiskWriteMode mode = DiskWriteMode::Direct;
        FileStream* stream = nullptr;
        size_t size = 0;

        // Sector index -> contents. Sparse: a boot dirties a few hundred, the worst case is the whole 60MB image.
        std::unordered_map<size_t, std::vector<uint8_t>> overlay;

        // The "this image is read only" complaint is worth making once, not once per sector.
        bool refusalLogged = false;
    };
};
