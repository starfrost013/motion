
/*
    C    O    H    E    R    E    N    T
    Extensible Emulator Debugging Tools!

    Coherent is an extensible debugger for emulators that is intended to allow the debugging of multiple types of CPU cores in an easy way.
*/

#pragma once
#include <Motion.hpp>
#include <base/emulation.hpp>
#include <component/addrspace.hpp>
#include <component/component.hpp>
#include <coherent/coherent_gui_imgui.hpp>

namespace Motion
{
    #define COHERENT_LOG_PREFIX     "Debugger"
    #define COHERENT_VERSION        "Coherent v0.7 (August 2026)"

    extern Cvar* startPaused;

    class CoherentCommand
    {
        char name[STRING_MAX_SHORT];
    };

    // enumerates types of coherent extensions
    enum CoherentExtensionType
    {
        /// @brief A type of extension that is added to the peripheral menu. The default value
        PeripheralsMenu = 0,

        /// @brief A custom menu.
        CustomMenu = 1,

        /// @brief A custom menu *item* on the main coherent menu
        CustomMenuItem = 2,
    };

    /// @brief defines a command extension object. all types that implement this must inherit from this class.
    /// the objects are automatically added to the menu
    class CoherentExtension
    {
        friend class Coherent;
        friend class CoherentUI;

    public:
        Component* component; 
        bool enabled = false; 

        CoherentExtension(Component* component)
        {
            this->component = component;
        }

        /// @brief Adds a command to a Coherent extension.
        /// @param command A pointer to teh command object to add.
        void AddCommand(CoherentCommand* command)
        {
            if (!command)
            {
                Logger::Log("CoherentExtension::AddCommand - command is nullptr", LogChannels::Error);
                return;
            }

            commands.push_back(command);
        }

        /// @brief Add the UI for a Coherent extension. Based on the type of the UI it either gets added to the peripherals menu, as a
        /// custom menu or as a customm enu item
        virtual void AddUI() { };

        // Getters for private methods
        virtual CoherentExtensionType GetExtensionType() { return CoherentExtensionType::PeripheralsMenu; };

        /// @brief Set the menu name. If this is not called the component name will be used as the menu name.
        /// @param name The menu name to use
        virtual const char* GetMenuName() { return "Name this Menu"; };

        // Setters for private methods

    private:
        std::vector<CoherentCommand*> commands;
    };

    /// @brief Defines a coherent system. A system is e.g. a CPU which is being debugged
    class CoherentSystem
    {
    public: 

        /// the fundamental word size of the processor
        enum WordSize
        {
            WordSize8 = 0x0,
            WordSize16 = 0x1,
            WordSize32 = 0x2,
            WordSize64 = 0x3,
        };

        // BASE CLASS for exception vector
        class ExceptionVectorBase
        {
        public: 
            const char* name; 
        };

        // exception vectors can be different sizes
        template <typename T>
        class ExceptionVector : public ExceptionVectorBase
        {
            T id; 
        public:
            ExceptionVector(const char* name, T id)
            {
                this->id = id;
                this->name = name;
            }
        };

        // We can allow the user to write custom implementations of the Register class with this.
        // Member templates are not allowed for variables, so provide a common base and make the templated register inherit from it. 
        // Registers are stored type-erased in the 'registers' map below.
        class RegisterBase
        {
        public:
            const char* name;

            virtual std::any Read() = 0; 
            virtual void Write(std::any& value) = 0;
        };

        template <typename T>
        class Register : public RegisterBase
        {
        public:
            Register(T* value, const char* name)
            {
                this->name = name;
                this->value = value;
            }
            
            /// @brief This DEREFERENCES the value of the register
            /// @return the register value
            std::any Read() override { return *value; };

            /// @brief Write the register
            /// @param value The register value to write - *MUST* Be a pointer. It gets converted to a pointer automatically so make sure it isn't automatically destroyed
            void Write(std::any& value) override { this->value = std::any_cast<T>(&value); }; 
        private: 
            T* value; 

        };

        /// @brief Disassemble a range of instructions.
        /// @param start The instrruction to disassemble.
        /// @param end The instruction to stop disassembling at.
        /// @return note: If you provide an unaligned instruction, it will just stop before the end. It's up to you to figure out the buffer size.
        virtual char* DisasmInstruction(size_t start) { return nullptr; };

        /// @brief Get the Program counter
        /// @return The program counter of the current system.
        virtual size_t GetPC() { return 0; };

        /// @brief enumerates the run states of the system
        enum RunState
        {
            Running = 0,
            Paused = 1,
            Reset = 2,
            SingleStep = 3,

            // not yet started i.e. don't display the stack etc. from the emulator's pov, this is the same as paused.
            NotYetStarted = 4,
        };

        /// @brief Add a register to this system
        /// @tparam T The type of the register to add.
        /// @param reg The Register<T> object to ad.
        /// @param name The friendly name of the register.
        template <typename T>
        void AddRegister(Register<T>* reg)
        {
            Logger::Log(std::format("CoherentSystem::AddRegister - Adding register with name {}", reg->name).c_str(), LogChannels::Debug);
            registers.push_back(reg); 
        }

        void Shutdown()
        {
            // don't bother cleaning these up on shutdown for now since the entire process is going away
            //for (auto* reg : registers)
                //delete reg;

            registers.clear();
        }

        /// @brief might be slow. this really needs to have a custom access only iterators.
        std::vector<RegisterBase*> registers;

        /// getters for private fields
        size_t GetNextInstructionSize() { return nextInstructionSize; };
        /// @brief get the run state of the system
        CoherentSystem::RunState GetRunState() { return runState; };
        CoherentSystem::WordSize GetWordSize() { return wordSize; };

        /// setters for private fields

        /// @brief set the run state of the system
        void SetRunState(CoherentSystem::RunState runState);

        // private because they may do something later
        void SetWordSize(CoherentSystem::WordSize wordSize) { this->wordSize = wordSize; };

        // we can't override templated virtual methods and this class is not really set up well for type erasure.

        virtual uint8_t GetStack8(uint32_t offset) 
        { 
            Logger::Log(COHERENT_LOG_PREFIX, "Coherent tried to call GetStack8 on a system but it didn't implement it. Check the word size");
            return 0xFF;
        }
        
        virtual uint16_t GetStack16(uint32_t offset) 
        { 
            Logger::Log(COHERENT_LOG_PREFIX, "Coherent tried to call GetStack16 on a system but it didn't implement it. Check the word size");
            return 0xFFFF;
        }

        virtual uint32_t GetStack32(uint32_t offset) 
        { 
            Logger::Log(COHERENT_LOG_PREFIX, "Coherent tried to call GetStack32 on a system but it didn't implement it. Check the word size");
            return 0xFFFFFFFF;
        }

        virtual uint64_t GetStack64(uint32_t offset) 
        { 
            Logger::Log(COHERENT_LOG_PREFIX, "Coherent tried to call GetStack64 on a system but it didn't implement it. Check the word size");
            return (uint64_t)-1; // bignumber of fs
        }
    protected: 

        inline static WordSize wordSize; 
        /// @brief the run state of the system
        inline static RunState runState;
        inline static size_t nextInstructionSize;
    };

    class Coherent
    {
        friend class CoherentUI;

    public: 
    
        /// @brief Initialise the coherent system
        static void Init();

        /// @brief Enters the Coherent system on command.
        static void Enter();
        
        /// @brief Tick the debugger. Called before all emulation components are ticked.
        static void Tick();
        
        /// @brief Render a frame of the debugger (see coherent_gui.cpp)
        static void Frame();
        
        /// @brief Called when the coherent system entered a breakpoint.
        static void OnBreakpointHit() 
        {
            // breakpoint is hit pause the system
            currentSystem->SetRunState(CoherentSystem::RunState::Paused);
        }

        static void Exception(uint32_t exception);

        /// This is the base class for all types of guards.
        class Guard
        {
        public: 
            size_t addr; 
            bool enabled;
            bool active; 

            /// @brief help for ui. set to true if the user selected this
            bool selected; 

            Guard()
            {
                this->addr = 0x0;
                this->enabled = false;
                this->active = false; 
                this->selected = false; 
            }
        
            Guard(size_t addr) : Guard()
            {
                this->addr = addr;
            }
        };
        

        /// defines a breakpoint
        class Breakpoint : public Guard
        {
        public:
            Breakpoint() : Guard() { }
            Breakpoint(size_t addr) : Guard(addr) { }
        };

        class Watchpoint : public Guard
        {
        public: 
            Watchpoint() : Guard() { }
            Watchpoint(size_t addr) : Guard(addr) { }

            // TODO: Add templates for these & use std::any
            uint32_t GetValue() { return AddrSpace::ReadU32(addr); }; 
        }; 

        /// @brief Called when the coherent system was requested to remove a breakpoint.
        static void AddBreakpoint(Breakpoint bp);
        static void AddWatchpoint(Watchpoint wp);

        /// @brief Called when the coherent system was requested to remove a breakpoint.
        static void RemoveBreakpoint(Breakpoint bp);
        static void RemoveWatchpoint(Watchpoint wp);

        static Breakpoint GetBreakpointByAddr(size_t addr);
        static Watchpoint GetWatchpointByAddr(size_t addr);

        // @brief Exit the coherent system.
        static void Leave();

        /// @brief SHut down the coherent system.
        static void Shutdown();

        /// @brief Register a coherent extension.
        /// @param extension A pointer to a valid CoherentExtension* object
        static void RegisterExtension(CoherentExtension* extension);

        // Getters for private members
        static bool GetInitialised() { return initialised; };
        static CoherentSystem* GetSystem() { return currentSystem; };

        // Setters for private members
        static void SetSystem(CoherentSystem* system) 
        { 
            currentSystem = system; 
        
            // start pausd if we configured to do so (for debugging)
            if (currentSystem != nullptr && startPaused->GetValue())
            {
                // add a new not yet started state
                currentSystem->SetRunState(CoherentSystem::RunState::NotYetStarted);
            }
        }; 
        
        /// @brief If this is true, the coherent system is currently active. (needs to be public because of imgui)
        inline static bool active;


    private:
        /// @brief If this value is true, the coherent system has been initialised. 
        inline static bool initialised;

        /// @brief the list of extensions
        /// NOTE: COherent will just clear its list. It's up to your component to delete the extension pointer.
        inline static std::vector<CoherentExtension*> extensions;

        /// @brief the current coherent system
        inline static CoherentSystem* currentSystem;

        // key is the size_t
        inline static std::unordered_map<size_t, Breakpoint> breakpoints;
        inline static std::unordered_map<size_t, Watchpoint> watchpoints;

        // automatically break on exception fired
        inline static bool breakOnException;
    };
}
