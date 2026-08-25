

/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    profile.hpp : yet another damn static class for the fgod damn configuraiton system !!!
    this is a wrapper around filesystem that redirects specified certain specific file writes, to a folder, 
    the name of which is based on a cvar.
*/

#pragma once
#include <Motion.hpp>
#include <base/filesystem/filesystem.hpp>
#include <base/filesystem/disk_image.hpp>
#include <platform/formats/ini.hpp>

namespace Motion
{
    // Common Configuration Convars
    // These are actually defined in the files specifically for them.
    extern Cvar* profileFolder; 
    extern Cvar* profileDisk0Path;
    extern Cvar* profileDisk1Path; // for future expansion
    extern Cvar* diskWriteMode;
    extern Cvar* diskCommitOnExit;
    extern Cvar* diskController;

    extern Cvar* ramInstalled;
    extern Cvar* numBitplanes;
    extern Cvar* vidScale;
    extern Cvar* machineName;
    extern Cvar* logChannels;
    extern Cvar* logDestinations;
    extern Cvar* startPaused;
    extern Cvar* skipLauncher;
    
    #define PROFILE_LOG_PREFIX      "Profile"

    /*
        Which disk controller is fitted. A machine has one - the DSD 5217 makes it a 3115 and the
        Interphase Storager makes it a 3130, and the two are alternatives rather than a pair, which is
        why this is one string rather than an enable flag per board. profileDisk0Path and
        profileDisk1Path are then the two physical drives hanging off whichever one it is: md0/md1 on
        the DSD, si0/si1 on the Storager.
    */
    enum class DiskControllerType
    {
        None,
        DSD5217,
        Storager,
    };

    // Base Configuration cvars

    class Profile
    {
    public: 
        /// @brief Initialises the profile system, currently just gets the profileFolder convar.
        static void Init();

        /// @brief Initialises system configuration convars for launcher
        static void InitCvars();

        /// @brief Builds a string with a profile folder path.
        /// @param fileName The file name to build. It will be automatically appended to the profileFolder convar.
        /// @param buf Must be at least 260 characters !!!!!
        static void GetProfileFolderPath(const char* fileName, char* buf);

        /// @brief open a file (see Filesystem::File)
        /// @param path the path to open
        /// @param mode the mode to open the file with
        /// @param skipProfileAddition if this is true, a temporary bufer of the right size will be created and passed into filesystem::open
        static FileStream* Open(const char* path, FileFlags mode = FileFlags::Text, bool skipProfileAddition = false);

        /// @brief Open a drive as a DiskImage, honouring +set diskWriteMode so every controller gets the same overlay.
        static DiskImage* OpenDisk(int32_t id);

        /// @brief Close a disk, committing its overlay if +set diskCommitOnExit says so. YOU MUST SET TO NULLPTR.
        static void CloseDisk(DiskImage* image);

        /// @brief Which disk controller +set diskController asks for. Warns once if it is nonsense.
        static DiskControllerType GetDiskController();

        static const char* DiskControllerName(DiskControllerType type);
        
        /// @brief this method does the same thing as Filesystem::Close. YOU MUST SET TO NULLPTR, your stream is DEAD
        static void Close(FileStream* fs);
    }; 
}; 
