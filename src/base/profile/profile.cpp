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

    FileStream* Profile::OpenDisk(int32_t id, FileFlags flags)
    {
        FileStream* hdd;
        const char* hddPath; // for debug

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
        
        Logger::Log(PROFILE_LOG_PREFIX, std::format("Opening HDD {} at {}...", id, hddPath).c_str());

        hdd = Profile::Open(hddPath, flags);

        if (!hdd)
        {
            Logger::Log(PROFILE_LOG_PREFIX, std::format("Failed to open HDD {} at {}!", id, hddPath).c_str(), LogChannels::Error);
            return nullptr; 
        }

        return hdd;
    }

    void Profile::Close(FileStream* fs)
    {
        return Filesystem::Close(fs);
    }
}

