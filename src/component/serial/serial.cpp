/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    serial.cpp: Serial I/O (see serial.hpp)
*/

#include <component/serial/serial.hpp>
#include <coherent/coherent_editor.hpp>

namespace Motion
{
    void SerialLine::SendByte(uint8_t data)
    {
        // send something to use the tx log

        txLog += (char)data;

        if (txLog.size() > SERIAL_TXLOG_MAX_SIZE)
            txLog.erase(0, SERIAL_TXLOG_PURGE_SIZE);

        // Also put it in the log a line at a time.
        if (data == '\n' || data == '\r')
        {
            if (!pendingLine.empty())
            {
                Logger::Log(SERIAL_LOG_PREFIX, std::format("[line {}] {}", id, pendingLine).c_str());
                CheckConsoleTrigger();
                pendingLine.clear();
            }
        }
        else
        {
            // printable only - the console carries the odd control byte and terminal escape
            if (data >= 0x20 && data < 0x7F)
                pendingLine += (char)data;

            if (pendingLine.size() >= SERIAL_LOG_LINE_MAX)
            {
                Logger::Log(SERIAL_LOG_PREFIX, std::format("[line {}] {}", id, pendingLine).c_str());
                pendingLine.clear();
            }
        }

        FireTransmitEvent(data);
    }

    // The interesting moment in a boot is usually one you cannot click a menu fast enough to catch, and there is no menu at all when running headless.
    void SerialLine::CheckConsoleTrigger()
    {
        if (!dumpOnConsoleMatch)
            dumpOnConsoleMatch = Cvar::Get("dumpOnConsoleMatch", "");

        const char* match = dumpOnConsoleMatch->GetString();

        if (!match || !*match || dumped)
            return;

        if (pendingLine.find(match) == std::string::npos)
            return;

        // once only - a panic tends to repeat and 16MB a time adds up fast
        dumped = true;
        CoherentEditor::DumpAll(std::format("console line matched \"{}\": {}", match, pendingLine).c_str());
    }

    void SerialLine::ClearTxLog()
    {
        txLog.clear();
    }

    bool SerialLine::TryReceiveByte(uint8_t& data)
    {
        {
            std::lock_guard<std::mutex> lock(rxQueueMutex);

            if (rxQueue.empty())
                return false;

            data = rxQueue.front();
            rxQueue.pop();
        }

        // Outside the lock - the event handlers are not ours and have no business holding it.
        FireReceiveEvent(data);
        return true;
    }

    void SerialLine::AddRxByte(uint8_t data)
    {
        std::lock_guard<std::mutex> lock(rxQueueMutex);
        rxQueue.push(data);
    }

    void SerialLine::AddRxString(const char* str)
    {
        if (!str)
            return;

        for (const char* c = str; *c; c++)
            AddRxByte((uint8_t)*c);
    }

    void SerialLine::FireReceiveEvent(uint8_t data)
    {
        SerialReceiveEvent event = SerialReceiveEvent();
        event.lineId = id;
        event.data = data; 

        EventSystem::FireEvent(event);
    }

    void SerialLine::FireTransmitEvent(uint8_t data)
    {
        SerialTransmitEvent event = SerialTransmitEvent();
        event.lineId = id;
        event.data = data; 

        EventSystem::FireEvent(event);
    }
}
