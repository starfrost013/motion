/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    serial.hpp: Serial I/O

    This is a very basic connectionless implementation of a single serial line; it doe snot know anything about baud rate, data framing or any other 
    chip behaviour which would be specific to a UART. This just bitbangs the raw data to a queue which can be read by some otehr component.
*/

#pragma once
#include <base/event/event.hpp>
#include <component/component.hpp>

#include <mutex>

namespace Motion
{
    /// @brief The number of raw serial lines a ComponentSerial can expose. Sized for two DUART chips, two
    /// channels each.
    #define SERIAL_MAX_LINES        4

    #define SERIAL_LOG_PREFIX       "Emulation - Serial"

    // Cap on how much transmitted output a SerialLine remember,s for display purposes.
    #define SERIAL_TXLOG_MAX_SIZE   16384
    #define SERIAL_TXLOG_PURGE_SIZE 4096

    // Console output is echoed to the log a line at a time; this caps a line with no newline in it.
    #define SERIAL_LOG_LINE_MAX     200

    /// @brief a serial receive event
    class SerialReceiveEvent : public Event
    {
    public:
        uint8_t data; 
        int32_t lineId;

        SerialReceiveEvent() : Event(EventType::SerialReceive) { };
    };

    class SerialTransmitEvent : public Event
    {
    public:
        uint8_t data; 
        int32_t lineId;
        
        SerialTransmitEvent() : Event(EventType::SerialTransmit) { };
    };

    /// @brief A single byte oriented raw serial line.
    ///
    /// Every SerialLine also keeps a small text log of everything it has sent
    /// (see GetTxLog()), or  injected into its receive queue directly (see AddRxByte()/AddRxString()) .
    class SerialLine
    {
    public:
        /// @brief Send a single raw byte out of this line (e.g. from a UART transmitter) to whatever it's connected to.
        void SendByte(uint8_t data);

        /// @brief Try to receive a single raw byte from whatever this line is connected to (e.g. into a UART receiver).
        /// @param data the data written with the received byte if one was available.
        /// @return true if a byte was available and has been written to 'data'.
        bool TryReceiveByte(uint8_t& data);

        /// @brief Push a single byte into this line's receive queue.
        void AddRxByte(uint8_t data);

        /// @brief Convenience wrapper around AddRxByte() for a whole NULL termianted string.
        void AddRxString(const char* str);

        /// @brief Clear the transmit log
        void ClearTxLog();

        // NOTE: this might be too slow because we have to tell everything about this event.
        // we can design a custom event system for serial ports if this turns out to be the case. but since serial ports run at low clock it is probably fine
        /// @brief Dump every memory editor if this line matches dumpOnConsoleMatch.
        void CheckConsoleTrigger();

        void FireReceiveEvent(uint8_t data);
        void FireTransmitEvent(uint8_t data);
        
        // Getters for private fields 

        /// @brief Get everything the current line has sent so far, for display purposes (e.g. the Coherent debug UI).
        std::string& GetTxLog() { return txLog; };

        // Lets this serial port know what we want
        int32_t id; 

    private:

        // Everything sent out of this line ends up here too, capped to SERIAL_TXLOG_MAX_SIZE, purely so it can be
        // displayed somewhere (e.g. Coherent) even when there's no real console attached to stdio.

        // At this point i just gave up and used std::string
        std::string txLog;

        // Whatever has been sent since the last newline, waiting to be echoed to the log.
        std::string pendingLine;

        inline static Cvar* dumpOnConsoleMatch = nullptr;
        inline static bool dumped = false;

        /*
            Receive queue - fed directly via AddRxByte()/AddRxString().

            Everything that types at a line does so from the render thread (the debugger's serial
            console, consoleInput), while the chip drains the queue from the emulation thread, so
            the two ends genuinely do run at once and the queue has to be guarded. Without this the
            pushes and the pop race and characters go missing, which reads as a flaky terminal
            rather than as a bug.
        */
        std::queue<uint8_t> rxQueue;
        std::mutex rxQueueMutex;


    };

    /// @brief Base class for components that expose one or more raw serial lines to the host (e.g. UART chips).
    /// Only handles getting bytes to/from the host chip
    class ComponentSerial : public Component
    {
    public:
        void Start() 
        {
            Component::Start();

            for (int32_t i = 0; i < SERIAL_MAX_LINES; i++)
                lines[i].id = i;
        }

        /// @brief Get one of this component's raw serial lines.
        /// @param lineNum The line number to get (e.g. DUART chip/channel index).
        SerialLine& GetLine(int32_t lineNum) { return lines[lineNum % SERIAL_MAX_LINES]; };

    protected:
        SerialLine lines[SERIAL_MAX_LINES];
    };
}
