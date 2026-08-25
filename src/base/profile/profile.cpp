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
    Cvar* diskController;

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

    DiskControllerType Profile::GetDiskController()
    {
        const char* wanted = diskController->GetString();

        if (!strcmp(wanted, "dsd") || !strcmp(wanted, "dsd5217"))
            return DiskControllerType::DSD5217;

        if (!strcmp(wanted, "storager") || !strcmp(wanted, "sii"))
            return DiskControllerType::Storager;

        if (!strcmp(wanted, "none"))
            return DiskControllerType::None;

        // Both controllers ask, so only complain the first time.
        static bool complained = false;

        if (!complained)
        {
            complained = true;

            Logger::Log(PROFILE_LOG_PREFIX, std::format("diskController \"{}\" is not one of dsd, storager or none - "
                "fitting the DSD 5217", wanted).c_str(), LogChannels::Warning);
        }

        return DiskControllerType::DSD5217;
    }

    const char* Profile::DiskControllerName(DiskControllerType type)
    {
        switch (type)
        {
        case DiskControllerType::DSD5217:
            return "DSD 5217";
        case DiskControllerType::Storager:
            return "Interphase Storager";
        default:
            return "none";
        }
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

        // DiskImage takes the path as given, so resolve it against the profile folder here.
        char resolved[STRING_MAX_PATH] = {0};
        GetProfileFolderPath(hddPath, resolved);

        if (!std::filesystem::exists(profileFolder->GetString()))
            std::filesystem::create_directory(profileFolder->GetString());

        Logger::Log(PROFILE_LOG_PREFIX, std::format("Opening HDD {} at {} ({})",
            id, hddPath, DiskImage::ModeName(mode)).c_str());

        DiskImage* image = DiskImage::Open(resolved, mode);

        if (!image)
        {
            // An empty second bay is normal, so only shout about an image that is there and will not open.
            bool exists = std::filesystem::exists(resolved);

            Logger::Log(PROFILE_LOG_PREFIX, std::format("HDD {}: {}", id, exists
                ? std::format("{} exists but could not be opened", hddPath)
                : std::format("no {} - that drive bay is empty", hddPath)).c_str(),
                exists ? LogChannels::Error : LogChannels::Message);

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

