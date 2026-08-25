/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    keyboard_iris.hpp: It's the keyboard.
    Generic keyboard interface

    On IRIS and early IRIS 4D, the keyboard I/O Interface is DUART0 Channel A. So we will hook to this. But this is the generic keyboard interface.#
    This class mostly exists so Machine::FindComponentByType can find all types of keyboards.
*/

#pragma once
#include <base/event/event.hpp>
#include <component/keyboard/keyboard.hpp>
// since this is DUART A Channel 0 we can basically do this by injecting data into that channel
#include <component/ip2/ip2_duart.hpp> 

namespace Motion
{
    // this is here because the presence of the keyboard is how the serial monitor gets entered into.

    // "Who are you".
    #define KEYBOARD_WHO_ARE_YOU_0      0x00
    #define KEYBOARD_WHO_ARE_YOU_1      0x10

    #define KEYBOARD_TYPE_IRIS          0xAA

    #define KEYBOARD_TYPE_4D60          0x6E        // *** TODO: 68K PROM Version 3.0.11 and later ONLY ***
    #define KEYBOARD_SUBTYPE_4D60_ISO   0x00        // *** TODO: 68K PROM Version 3.0.11 and later ONLY ***
    #define KEYBOARD_SUBTYPE_4D60_STD   0x01        // *** TODO: 68K PROM Version 3.0.11 and later ONLY ***

    #define KEYBOARD_DUART_LINE         0           // DUART0 Channel A
    #define KEYBOARD_LOG_PREFIX         "Keyboard"

    extern Cvar* logKeyboard;
    #define KEYBOARD_NUM_KEYS           111

    // Modifers
    #define KEYBOARD_STATE_LEFT_SHIFT   (1 << 0)    
    #define KEYBOARD_STATE_RIGHT_SHIFT  (1 << 1)
    #define KEYBOARD_STATE_LEFT_CTRL    (1 << 2)
    #define KEYBOARD_STATE_RIGHT_CTRL   (1 << 3)
    #define KEYBOARD_STATE_LEFT_ALT     (1 << 4)
    #define KEYBOARD_STATE_RIGHT_ALT    (1 << 5)
    #define KEYBOARD_STATE_CAPSLOCK     (1 << 6)
    #define KEYBOARD_STATE_SETUP        (1 << 7)

    // The scancode a key press puts on the wire is the key number with bit 7 telling the host whether it went down or came up, and IRIX reads it as #define.
    #define KEYBOARD_SCANCODE_KEY_MASK  0x7F
    #define KEYBOARD_SCANCODE_RELEASE   0x80
    #define KEYBOARD_SCANCODE_COUNT     128

    class KeyboardIris : public ComponentKeyboard
    {
    public: 
        // This is from GL2
        struct Key
        {
            uint8_t normal;                 // normal
            uint8_t shift;                  // shift
            uint8_t control;                // control
            uint8_t controlShift;           // ctrl+shifted ascii index
            uint8_t alt;                    // alt index
            uint8_t gs;                     // no idea what this is
        };

        virtual const char* GetName() override { return "IRIS Keyboard"; }; 
        void OnEvent(Event& evt) override;
        void Shutdown() override;
    private: 
        bool LookupScancode(uint32_t key, uint8_t& scancode);
        void SendKey(uint8_t scancode, bool down);
        void ReleaseEverything();

        DUART68681* duart;

        bool initialised = false;

        // The last byte the host sent, so the two byte "who are you" can be recognised as a pair.
        uint8_t lastHostByte = 0xFF;

        // shut up the log files
        bool shutUpBeep = false; 
        bool shutUpLed = false; 

        // Which keys the guest currently believes are held.
        bool keyDown[KEYBOARD_SCANCODE_COUNT] = {};

        // maps SDL key events to SGI keycodes
        // TODO: PF1-PF4, SETUP etc
        inline static std::unordered_map<SDL_Keycode, uint8_t> sdlToSgi =
        {
            { SDLK_ESCAPE,        6 },
            { SDLK_1,             7 },
            { SDLK_2,            13 },
            { SDLK_3,            14 },
            { SDLK_4,            21 },
            { SDLK_5,            22 },
            { SDLK_6,            29 },
            { SDLK_7,            30 },
            { SDLK_8,            37 },
            { SDLK_9,            38 },
            { SDLK_0,            45 },
            { SDLK_Q,             9 },
            { SDLK_A,            10 },
            { SDLK_S,            11 },
            { SDLK_W,            15 },
            { SDLK_E,            16 },
            { SDLK_D,            17 },
            { SDLK_F,            18 },
            { SDLK_Z,            19 },
            { SDLK_X,            20 },
            { SDLK_R,            23 },
            { SDLK_T,            24 },
            { SDLK_G,            25 },
            { SDLK_H,            26 },
            { SDLK_C,            27 },
            { SDLK_V,            28 },
            { SDLK_Y,            31 },
            { SDLK_U,            32 },
            { SDLK_J,            33 },
            { SDLK_K,            34 },
            { SDLK_B,            35 },
            { SDLK_N,            36 },
            { SDLK_I,            39 },
            { SDLK_O,            40 },
            { SDLK_L,            41 },
            { SDLK_M,            43 },
            { SDLK_P,            47 },
            { SDLK_SEMICOLON,    42 },
            { SDLK_COMMA,        44 },
            { SDLK_MINUS,        46 },
            { SDLK_LEFTBRACKET,  48 },
            { SDLK_APOSTROPHE,   49 },
            { SDLK_RETURN,       50 },
            { SDLK_PERIOD,       51 },
            { SDLK_SLASH,        52 },
            { SDLK_EQUALS,       53 },
            { SDLK_GRAVE,        54 },
            { SDLK_RIGHTBRACKET ,55 },
            { SDLK_BACKSLASH,    56 },
            { SDLK_KP_1,         57 },
            { SDLK_KP_0,         58 },
            { SDLK_KP_ENTER,     81 },
            { SDLK_KP_4,         62 },
            { SDLK_KP_2,         63 },
            { SDLK_KP_3,         64 },
            { SDLK_KP_PERIOD,    65 },
            { SDLK_KP_7,         66 },
            { SDLK_KP_8,         67 },
            { SDLK_KP_5,         68 },
            { SDLK_KP_6,         69 },
            // Buttons 70, 71, 77 and 78 are the keypad's own PF1 to PF4, which a PC keypad does not have.
            { SDLK_KP_9,         74 },
            { SDLK_KP_MINUS,     75 },
            { SDLK_KP_COMMA,     76 },
            { SDLK_TAB,           8 },
            { SDLK_BACKSPACE,    60 },
            { SDLK_DELETE,       61 },
            { SDLK_SPACE,        82 },

            { SDLK_LEFT,         72 },
            { SDLK_DOWN,         73 },
            { SDLK_RIGHT,        79 },
            { SDLK_UP,           80 },

            { SDLK_LCTRL,         2 },
            { SDLK_CAPSLOCK,      3 },
            { SDLK_RSHIFT,        4 },
            { SDLK_LSHIFT,        5 },

            { SDLK_LALT,         83 },
            { SDLK_RALT,         84 },
            { SDLK_RCTRL,        85 },

            { SDLK_F1,            86 },
            { SDLK_F2,            87 },
            { SDLK_F3,            88 },
            { SDLK_F4,            89 },
            { SDLK_F5,            90 },
            { SDLK_F6,            91 },
            { SDLK_F7,            92 },
            { SDLK_F8,            93 },
            { SDLK_F9,            94 },
            { SDLK_F10,           95 },
            { SDLK_F11,           96 },
            { SDLK_F12,           97 },

            { SDLK_PRINTSCREEN,   98 },
            { SDLK_SCROLLLOCK,    99 },
            { SDLK_PAUSE,        100 },

            { SDLK_INSERT,       101 },
            { SDLK_HOME,         102 },
            { SDLK_PAGEUP,       103 },
            { SDLK_END,          104 },
            { SDLK_PAGEDOWN,     105 },

            { SDLK_NUMLOCKCLEAR, 106 },

            { SDLK_KP_DIVIDE,    107 },
            { SDLK_KP_MULTIPLY,  108 },
            { SDLK_KP_PLUS,      109 },

            { SDLK_LESS,         110 },
        };
    };
}