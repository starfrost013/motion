/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    disk_image.cpp: see disk_image.hpp for what the three modes are for.
*/

#include <base/filesystem/disk_image.hpp>

namespace Motion
{
    DiskWriteMode DiskImage::ModeFromString(const char* name)
    {
        if (!strcmp(name, "direct"))
            return DiskWriteMode::Direct;

        if (!strcmp(name, "overlay"))
            return DiskWriteMode::Overlay;

        if (!strcmp(name, "readonly"))
            return DiskWriteMode::ReadOnly;

        Logger::Log(DISK_IMAGE_LOG_PREFIX, std::format("diskWriteMode \"{}\" is not one of direct, overlay or "
            "readonly - using direct", name).c_str(), LogChannels::Warning);

        return DiskWriteMode::Direct;
    }

    const char* DiskImage::ModeName(DiskWriteMode mode)
    {
        switch (mode)
        {
        case DiskWriteMode::Overlay:
            return "overlay";
        case DiskWriteMode::ReadOnly:
            return "read-only";
        default:
            return "direct";
        }
    }

    DiskImage* DiskImage::Open(const char* path, DiskWriteMode mode)
    {
        // Overlay and ReadOnly open read-only, which is the whole guarantee - and is why two emulators on one image stop mattering.
        std::ios_base::openmode flags = std::ios_base::in | std::ios_base::binary;

        if (mode == DiskWriteMode::Direct)
            flags |= std::ios_base::out;

        FileStream* stream = new FileStream;
        stream->stream.open(path, flags);

        if (stream->stream.fail())
        {
            delete stream;
            return nullptr;
        }

        stream->stream.seekg(0, std::ios_base::end);

        size_t size = (size_t)stream->stream.tellg();
        stream->stream.clear();

        DiskImage* image = new DiskImage;

        image->path = path;
        image->mode = mode;
        image->stream = stream;
        image->size = size;

        return image;
    }

    DiskImage::~DiskImage()
    {
        // Say what is being thrown away - losing a megabyte silently is how somebody learns diskCommitOnExit exists too late.
        if (mode == DiskWriteMode::Overlay
        && !overlay.empty())
        {
            Logger::Log(DISK_IMAGE_LOG_PREFIX, std::format("{}: discarding {} sectors ({} KB) written by the guest - "
                "+set diskCommitOnExit 1 keeps them", path, overlay.size(),
                (overlay.size() * DISK_IMAGE_SECTOR_SIZE) / 1024).c_str());
        }

        if (stream)
        {
            Filesystem::Close(stream);
            stream = nullptr;
        }
    }

    bool DiskImage::ReadBase(size_t offset, uint8_t* buffer, size_t length)
    {
        // A short read latches eofbit/failbit and every later access silently does nothing, so clear first.
        stream->stream.clear();
        stream->stream.seekg(offset, std::ios_base::beg);
        stream->stream.read((char*)buffer, length);

        bool ok = ((size_t)stream->stream.gcount() == length);

        stream->stream.clear();
        return ok;
    }

    bool DiskImage::Read(size_t offset, uint8_t* buffer, size_t length)
    {
        if (!length)
            return true;

        if (offset >= size
        || length > (size - offset))
            return false;

        // Read the base span, then paint dirty sectors over it: one wasted read on an all-dirty span, in exchange for not tracking runs.
        if (!ReadBase(offset, buffer, length))
            return false;

        if (overlay.empty())
            return true;

        size_t firstSector = offset / DISK_IMAGE_SECTOR_SIZE;
        size_t lastSector = (offset + length - 1) / DISK_IMAGE_SECTOR_SIZE;

        for (size_t sector = firstSector; sector <= lastSector; sector++)
        {
            auto entry = overlay.find(sector);

            if (entry == overlay.end())
                continue;

            // The first and last sectors of the span may only be partly covered by it.
            size_t sectorStart = sector * DISK_IMAGE_SECTOR_SIZE;
            size_t from = (offset > sectorStart) ? (offset - sectorStart) : 0;
            size_t to = DISK_IMAGE_SECTOR_SIZE;

            if ((sectorStart + to) > (offset + length))
                to = (offset + length) - sectorStart;

            memcpy(buffer + (sectorStart + from) - offset, entry->second.data() + from, to - from);
        }

        return true;
    }

    uint8_t* DiskImage::MaterialiseSector(size_t sector)
    {
        auto entry = overlay.find(sector);

        if (entry != overlay.end())
            return entry->second.data();

        size_t sectorStart = sector * DISK_IMAGE_SECTOR_SIZE;

        if (sectorStart >= size)
            return nullptr;

        std::vector<uint8_t> contents(DISK_IMAGE_SECTOR_SIZE, 0);

        // Seed from the base first, so a partial write leaves the rest reading as the platter rather than as zeroes.
        size_t available = size - sectorStart;

        if (available > DISK_IMAGE_SECTOR_SIZE)
            available = DISK_IMAGE_SECTOR_SIZE;

        ReadBase(sectorStart, contents.data(), available);

        return overlay.emplace(sector, std::move(contents)).first->second.data();
    }

    bool DiskImage::Write(size_t offset, const uint8_t* buffer, size_t length)
    {
        if (!length)
            return true;

        if (offset >= size
        || length > (size - offset))
            return false;

        if (mode == DiskWriteMode::ReadOnly)
        {
            if (!refusalLogged)
            {
                refusalLogged = true;

                Logger::Log(DISK_IMAGE_LOG_PREFIX, std::format("{} is open read-only and the guest tried to write to "
                    "it - the write was refused and further ones will not be logged", path).c_str(), LogChannels::Warning);
            }

            return false;
        }

        if (mode == DiskWriteMode::Direct)
        {
            stream->stream.clear();
            stream->stream.seekp(offset, std::ios_base::beg);
            stream->stream.write((const char*)buffer, length);
            stream->stream.flush();

            bool ok = !stream->stream.fail();
            stream->stream.clear();

            return ok;
        }

        size_t firstSector = offset / DISK_IMAGE_SECTOR_SIZE;
        size_t lastSector = (offset + length - 1) / DISK_IMAGE_SECTOR_SIZE;

        for (size_t sector = firstSector; sector <= lastSector; sector++)
        {
            uint8_t* contents = MaterialiseSector(sector);

            if (!contents)
                return false;

            size_t sectorStart = sector * DISK_IMAGE_SECTOR_SIZE;
            size_t from = (offset > sectorStart) ? (offset - sectorStart) : 0;
            size_t to = DISK_IMAGE_SECTOR_SIZE;

            if ((sectorStart + to) > (offset + length))
                to = (offset + length) - sectorStart;

            memcpy(contents + from, buffer + (sectorStart + from) - offset, to - from);
        }

        return true;
    }

    bool DiskImage::Commit()
    {
        if (overlay.empty())
            return true;

        // A second handle, opened only now: the point of the mode is that nothing can write to the file until asked.
        std::fstream out(path, std::ios_base::in | std::ios_base::out | std::ios_base::binary);

        if (out.fail())
        {
            Logger::Log(DISK_IMAGE_LOG_PREFIX, std::format("Cannot reopen {} to commit {} sectors - the overlay has "
                "not been written and is still in memory", path, overlay.size()).c_str(), LogChannels::Error);

            return false;
        }

        size_t written = 0;

        for (const auto& [sector, contents] : overlay)
        {
            size_t sectorStart = sector * DISK_IMAGE_SECTOR_SIZE;
            size_t length = DISK_IMAGE_SECTOR_SIZE;

            if ((sectorStart + length) > size)
                length = size - sectorStart;

            out.seekp(sectorStart, std::ios_base::beg);
            out.write((const char*)contents.data(), length);

            if (out.fail())
            {
                Logger::Log(DISK_IMAGE_LOG_PREFIX, std::format("Commit of {} failed {} sectors in, at 0x{:x} - the "
                    "image is now part written and should be checked", path, written, sectorStart).c_str(),
                    LogChannels::Error);

                return false;
            }

            written++;
        }

        out.flush();
        out.close();

        Logger::Log(DISK_IMAGE_LOG_PREFIX, std::format("Committed {} sectors ({} KB) to {}",
            written, (written * DISK_IMAGE_SECTOR_SIZE) / 1024, path).c_str());

        overlay.clear();
        return true;
    }

    void DiskImage::Discard()
    {
        overlay.clear();
    }
};
