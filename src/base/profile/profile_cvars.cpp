/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    profile_cvars.cpp: We put common cvars here so the launcher and later the config loader can access them.
*/

#include <base/profile/profile.hpp>

namespace Motion
{
    void Profile::InitCvars()
    {
    // too lazy to snprintf
    #ifdef __MINGW32__
        profileFolder = Cvar::Get("profileFolder", ".\\profile");
    #else
        profileFolder = Cvar::Get("profileFolder", "./profile");
    #endif
        skipLauncher = Cvar::Get("skipLauncher", "0");
        vidScale = Cvar::Get("vidScale", "1");
        startPaused = Cvar::Get("startPaused", "1");
        ramInstalled = Cvar::Get("ramInstalled", "16777216");
        numBitplanes = Cvar::Get("numBitplanes", "32");
        profileDisk0Path = Cvar::Get("profileDisk0Path", "3130.img");
        profileDisk1Path = Cvar::Get("profileDisk1Path", "3130_2.img");

        /*
            direct is the default so that a machine still keeps what it writes. overlay is the one to
            reach for while developing: the image on disk is never touched, so it does not matter how
            the emulator stops, and it does not matter if two of them are running. See disk_image.hpp.
        */
        /*
            Which disk controller is fitted: dsd, storager or none. A machine has one, and which one
            it is decides what the machine *is* - the DSD makes it a 3115, the Storager a 3130.
            profileDisk0Path and profileDisk1Path are the two drives on whichever it is.
        */
        diskController = Cvar::Get("diskController", "dsd");

        diskWriteMode = Cvar::Get("diskWriteMode", "direct");
        diskCommitOnExit = Cvar::Get("diskCommitOnExit", "0");
    }
}; 