#include <base/filesystem/filesystem.hpp>

//
// It's a FIlesystem API! THERE IS NO HOPE! 
// THis one just provides a very narrow thing...
//

namespace Motion
{
    FileStream* Filesystem::Open(const char* path, FileFlags mode)
    {
        FileStream* fc = new FileStream;

        auto flags = std::ios_base::in | std::ios_base::out;

        //these should probably be exclusive
        if (mode & FileFlags::CreateOrOpen)
            flags |= std::ios_base::app;
        else if (mode & FileFlags::Create)
            flags = std::ios::out | std::ios::trunc;

        if (mode & FileFlags::Binary)
            flags |= std::ios_base::binary;
     
        fc->stream.open(path, flags);
    
        if (fc->stream.fail()
        || fc->stream.bad())
        {
            delete fc;
            //todo: fileflags for optional logging

            if (mode & FileFlags::LogOnFail)
                Logger::Log(std::format("Filesystem::Open with FileFlags::LogOnFail set - Failed to Open file {}, FileFlags {}", path, (size_t)mode).c_str());

            return nullptr;
        }

        // we don't have an open boolean because they are explicitly open until closed currently

        return fc; 

    }

    void Filesystem::Seek(FileStream* fs, size_t offset)
    {
        fs->stream.seekg(offset, std::ios_base::beg); // seekg == seekp
    }
    
    size_t Filesystem::Tell(FileStream* fs)
    {
        return fs->stream.tellg();
    }

    size_t Filesystem::GetSize(FileStream* fs)
    {
        fs->stream.clear(); // clear any failbits we may need
        fs->stream.seekg(0, std::ios_base::end);
        
        auto size = fs->stream.tellg();

        if (size == -1)
        {
            Logger::Log(FILESYSTEM_LOG_PREFIX, std::format("Somehow we failed to seek?!").c_str(), LogChannels::FatalError);
            return 0; // doesn't mater
        }

        return fs->stream.tellg();
    }

    bool Filesystem::IsEOF(FileStream* fs)
    {
        fs->eof = fs->stream.eof();
        fs->stream.clear(); //unset failbit so you can read again
        return fs->eof; 
    }

    void Filesystem::ReadString(FileStream* fs, char* buf, size_t bufLen, ReadStringType type = ReadStringType::CChar)
    {
        if (Filesystem::IsEOF(fs))
            return; 

        uint8_t realLen = 0; 
        size_t bufPtr = 0;

        switch (type)
        {
            case ReadStringType::Pascal:
                fs->stream.read((char*)(&realLen), sizeof(uint8_t));

                if (realLen > bufLen)
                {
                    Logger::Log(FILESYSTEM_LOG_PREFIX, std::format("Filesystem::ReadString with ReadStringType::Pascal: actual string length {} was greater than buffer size {}, truncating", realLen, bufLen).c_str(), LogChannels::Warning);
                    realLen = bufLen;
                }

                if (realLen == 0)
                {
                    Logger::Log(FILESYSTEM_LOG_PREFIX, std::format("Filesystem::ReadString with ReadStringType::Pascal: string length is 0, nothing to do", realLen, bufLen).c_str(), LogChannels::Warning);
                    return; 
                }

                fs->stream.read(buf, realLen);
                break; 
            case ReadStringType::CChar:
                for (int32_t i = 0; i < bufLen; i++)
                {
                    fs->stream.read((char*)(buf + bufPtr), sizeof(char));

                    if (buf[bufPtr] == '\0')
                        break; // hit end of string

                    bufPtr++;
                }

                Logger::Log(FILESYSTEM_LOG_PREFIX, std::format("Filesystem::ReadString with ReadStringType::CChar: no terminator before bufLen reached!").c_str(), LogChannels::Warning);
            
                break;
        }
    }

    void Filesystem::Close(FileStream* fs)
    {
        // YOU MUST SET TO NULLPTR!
        delete fs; 
    }
}