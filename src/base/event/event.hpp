
/*
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    event.hpp: A little backend independent arbitrary event system
*/

#pragma once
#include <Motion.hpp>

namespace Motion
{
    enum EventType
    {
        KeyDown = 0,
        KeyUp = 1,
        MouseDown = 2,
        MouseUp = 3,

        // for convenience the event classes for these are deifned in the serial code
        // i think the name of these events might need to be flipped.
        SerialReceive = 4,
        SerialTransmit = 5,

        // The window stopped being the one the host is typing at. Anything that tracks which keys
        // are held has to let go of them here, because the key-up that would have done it is going
        // to be delivered to whatever the user switched to instead.
        FocusLost = 6,

        // The host pointer moved. Relative, because that is all the emulated hardware can express:
        // the IRIS mouse is a quadrature encoder that reports transitions, not a position.
        MouseMotion = 7,
    }; 

    extern const char* eventTypeToString[];
    
    class Event
    {
    public: 
        EventType type;    
        
        Event(EventType type)
        { 
            this->type = type; 
        }
    };
    
    // some prebuilt event types
  
    class KeyDownEvent : public Event
    {
    public:
        uint32_t key;
        uint32_t scancode;
        uint32_t mod;
        bool repeat;

        KeyDownEvent() : Event(EventType::KeyDown) { };
    }; 

    class FocusLostEvent : public Event
    {
    public:
        FocusLostEvent() : Event(EventType::FocusLost) { };
    };

    class KeyUpEvent : public Event
    {
    public:
        uint32_t key;
        uint32_t scancode;
        uint32_t mod;
        bool repeat;

        KeyUpEvent() : Event(EventType::KeyUp) { };
    }; 
    
    class MouseDownEvent : public Event
    {
    public:
        uint32_t mouse;
        uint8_t numClicks;

        MouseDownEvent() : Event(EventType::MouseDown) { };
    };

    class MouseUpEvent : public Event
    {
    public:
        uint32_t mouse;
        uint8_t numClicks;

        MouseUpEvent() : Event(EventType::MouseUp) { };
    };

    class MouseMotionEvent : public Event
    {
    public:
        // How far the pointer moved since the last event, in host pixels. The guest screen is the
        // same size as the window, so one host pixel is one mouse tick is one guest pixel.
        float deltaX;
        float deltaY;

        MouseMotionEvent() : Event(EventType::MouseMotion) { };
    };


    class EventSystem
    {
    public:
        static void FireEvent(Event& evt);
    private: 
    }; 
}; 