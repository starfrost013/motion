/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    profile.cpp : the implementation of...yet another damn static class for the fgod damn configuraiton system !!!
    this is a wrapper around filesystem that redirects specified certain specific file writes, to a folder, 
    the name of which is based on a cvar.
*/

#include <base/profile/profile.hpp>

namespace Motion
{
    Cvar* profileFolder; 
    Cvar* profileDisk0Path; 
    Cvar* profileDisk1Path; 
    Cvar* diskWriteMode;
    Cvar* diskCommitOnExit;

    void Profile::Init()
    {
        InitCvars();
    }

    void Profile::GetProfileFolderPath(const char* fileName, char* buf)
    {
        snprintf(buf, STRING_MAX_PATH, "%s%c%s", profileFolder->GetString(), std::filesystem::path::preferred_separator, fileName);
    }

    // helper methods

    FileStream* Profile::Open(const char* path, FileFlags mode, bool skipProfileAddition)
    {
        // Ensure there actually is a profile folder
        if (!std::filesystem::exists(profileFolder->GetString()))
        {
            std::filesystem::create_directory(profileFolder->GetString());
        }

        if (!skipProfileAddition)
        {
            // BOZO user (ME!) might not ensure a buffer of the right size!! so copy into a safe, temporary buffer first...TO SAVE THE MEMORY'S INTEGRITY!
            char antiBozoBuf[STRING_MAX_PATH] = {0};
            GetProfileFolderPath(path, antiBozoBuf);

            return Filesystem::Open(antiBozoBuf, mode);
        }
        else
            return Filesystem::Open(path, mode);

    }

    DiskImage* Profile::OpenDisk(int32_t id)
    {
        const char* hddPath;

        // I also don't like this code
        switch (id)
        {
            case 0:
                hddPath = profileDisk0Path->GetString();
                break; 
            case 1: 
                hddPath = profileDisk1Path->GetString();
                break;
            default:
                Logger::Log(PROFILE_LOG_PREFIX, "Profile::OpenDisk - Only 2 HDDs are supported!");
                return nullptr;
        }

        DiskWriteMode mode = DiskImage::ModeFromString(diskWriteMode->GetString());

        // Disks live in the profile folder like everything else, and DiskImage takes the path as
        // given, so resolve it here rather than inside it.
        char resolved[STRING_MAX_PATH] = {0};
        GetProfileFolderPath(hddPath, resolved);

        if (!std::filesystem::exists(profileFolder->GetString()))
            std::filesystem::create_directory(profileFolder->GetString());

        Logger::Log(PROFILE_LOG_PREFIX, std::format("Opening HDD {} at {} ({})",
            id, hddPath, DiskImage::ModeName(mode)).c_str());

        DiskImage* image = DiskImage::Open(resolved, mode);

        if (!image)
        {
            Logger::Log(PROFILE_LOG_PREFIX, std::format("Failed to open HDD {} at {}!", id, hddPath).c_str(), LogChannels::Error);
            return nullptr; 
        }

        if (mode == DiskWriteMode::Overlay)
            Logger::Log(PROFILE_LOG_PREFIX, std::format("HDD {} is copy-on-write: the image on disk will not be "
                "modified{}", id, diskCommitOnExit->GetValue() ? " unless the guest writes to it, which will be "
                "committed on exit" : ", whatever happens to the emulator").c_str());

        return image;
    }

    void Profile::CloseDisk(DiskImage* image)
    {
        if (!image)
            return;

        /*
            The one way an overlay survives the process. It is off by default because throwing the
            writes away is the entire point of the mode - this is for the boot where you meant it,
            like an /etc/fsck repair you want to keep.
        */
        if (image->GetMode() == DiskWriteMode::Overlay
        && diskCommitOnExit->GetValue()
        && image->GetDirtySectorCount())
        {
            image->Commit();
        }

        delete image;
    }

    void Profile::Close(FileStream* fs)
    {
        return Filesystem::Close(fs);
    }
}

