#pragma once

#include <Motion.hpp>
#include <filesystem>

//
// It's a FIlesystem API! THERE IS NO HOPE! 
// THis one just provides a very narrow thing...
//

#define FILESYSTEM_LOG_PREFIX           "Filesystem"

namespace Motion
{
    class FileStream
    {
    public: 
        // the stream of the file
        std::fstream stream;
        bool eof = false;
    };

    enum FileFlags
    {
        Text = 1,
        Binary = 1 << 1,
        Create = 1 << 2,
        CreateOrOpen = 1 << 3,
        LogOnFail = 1 << 4,
    }; 
    
    enum ReadStringType
    {
        CChar,                      // Read until zero or buffer exhausted (WARNING: YOU MUST PASS THE RIGHT BUFFER LENGTH, OR YOU'LL BE IN BIG TROUBLE!!!)
        Pascal,                     // Length first 
    };

    class Filesystem
    {
    public:
        static FileStream* Open(const char* path, FileFlags mode = FileFlags::Text);
        static void Seek(FileStream* fs, size_t offset);
        static size_t Tell(FileStream* fs);
        static size_t GetSize(FileStream* fs);

        /// @brief safer way of calling eof which does not lock teh stream
        /// @param fs  the filestream to checl
        /// @return a boolean indicating if we read everything
        static bool IsEOF(FileStream* fs);
        static void Close(FileStream* fs);

        static void ReadString(FileStream* fs, char* ptr, size_t len, ReadStringType type = ReadStringType::CChar);
    };
}; 