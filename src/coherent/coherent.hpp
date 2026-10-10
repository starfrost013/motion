
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
    #define COHERENT_VERSION        "Coherent v0.9"

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

        /// @brief Set the menu option name for custom menu type items. If this is not overridden the component name will be used as the menu name.
        /// @param name The menu name to use. Default is the componet name.
        virtual const char* GetMenuName() { return component->GetName(); };

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

        // the stack used for the stack window

        class StackBase
        {
        public: 
            virtual void Push(std::any value) = 0;
            virtual std::any At(size_t offset) = 0;
            virtual std::any Pop() = 0;
            virtual uint64_t Size() = 0;

        };

        template <typename T>
        class Stack : public StackBase
        {
        public: 
            void Push(std::any value) override
            {
                stack.push_back(std::any_cast<T>(value));
            }

            std::any Pop() override
            {
                if (stack.empty())
                    return std::any{};

                std::any first = stack.back();
                stack.pop_back();
                return first; 
            }

            std::any At(size_t offset) override
            {
                if (stack.empty())
                    return std::any{};

                if (offset >= stack.size())
                    return std::any{};
                    
                return stack.at(offset);
            }

            uint64_t Size() override
            {
                return stack.size();
            }

        private: 
            std::vector<T> stack;
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

            // this is a buffer where the value of this gets stored by the UI
            char valBuf[STRING_MAX_SHORT];

            virtual std::any Read() = 0; 
            virtual void Write(std::any value) = 0;
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
            /// @param value The register value to write. Must be an integer; it gets automatically converted to a uint64_t and then masekd of
            void Write(std::any value) override { *(this->value) = static_cast<T>(std::any_cast<uint64_t>(value)); }; 
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

        /// @brief enumerates the possible step types that we can use
        enum StepType
        {
            /// @brief a normal step type
            Normal = 0,

            /// @brief step over
            Over = 1,
        };

        /// @brief enumerates the run states of the system
        enum RunState
        {
            Running = 0,
            Paused = 1,
            Reset = 2,
            SingleStepNormal = 3,
            SingleStepOver = 4,
            
            // not yet started i.e. don't display the stack etc. from the emulator's pov, this is the same as paused.
            NotYetStarted = 5,
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

        uint64_t GetStackSize()
        {
            return GetStack().Size();
        }

        uint8_t GetStack8(uint32_t offset) 
        { 
            auto stackAt = GetStack().At(offset);

            if (stackAt.has_value())
                return std::any_cast<uint8_t>(stackAt);
            else
                return 0x00; // defualt 0
        }
        
        uint16_t GetStack16(uint32_t offset) 
        { 
            auto stackAt = GetStack().At(offset);

            if (stackAt.has_value())
                return std::any_cast<uint16_t>(stackAt);
            else
                return 0x00; // defualt 0
        }

        uint32_t GetStack32(uint32_t offset) 
        { 
            auto stackAt = GetStack().At(offset);

            if (stackAt.has_value())
                return std::any_cast<uint32_t>(stackAt);
            else
                return 0x00; // defualt 0
        }

        uint64_t GetStack64(uint32_t offset) 
        { 
            auto stackAt = GetStack().At(offset);

            if (stackAt.has_value())
                return std::any_cast<uint64_t>(stackAt);
            else
                return 0x00; // defualt 0
        }

        // evil very bad probably
        // the bridge between fixed-size cpu land and magical C++ templates! 

        void PushCall8(uint8_t offset) 
        { 
            GetStack().Push(std::any_cast<uint8_t>(offset)); 
        }
        
        void PushCall16(uint16_t offset) 
        { 
            GetStack().Push(std::any_cast<uint16_t>(offset)); 
        }

        void PushCall32(uint32_t offset) 
        { 
            GetStack().Push(std::any_cast<uint32_t>(offset)); 
        }

        void PushCall64(uint64_t offset) 
        { 
            GetStack().Push(std::any_cast<uint64_t>(offset)); 
        }

        uint8_t PopCall8(uint8_t offset) 
        { 
            auto item = GetStack().Pop();

            if (item.has_value()) // probably hould not ever be false
                return std::any_cast<uint8_t>(item);
            else
                return 0x00;
        }
        
        uint16_t PopCall16(uint16_t offset) 
        { 
            auto item = GetStack().Pop();

            if (item.has_value()) // probably hould not ever be false
                return std::any_cast<uint16_t>(item);
            else
                return 0x00;
        }

        uint32_t PopCall32(uint32_t offset) 
        { 
            auto item = GetStack().Pop();

            if (item.has_value()) // probably hould not ever be false
                return std::any_cast<uint32_t>(item);
            else
                return 0x00;        
            }

        uint64_t PopCall64(uint64_t offset) 
        { 
            auto item = GetStack().Pop();

            if (item.has_value()) // probably hould not ever be false
                return std::any_cast<uint64_t>(item);
            else
                return 0x00;
        }
        
        /// @brief by default coherent provides a 32 bit stack. if you want a different stack you need to create a unique_ptr of the stack
        /// @return the stack that your cpu will read and write to for the debugger stack window
        virtual StackBase& GetStack() { return *stack; };

    protected: 

        inline static WordSize wordSize; 
        /// @brief the run state of the system
        inline static RunState runState;
        inline static size_t nextInstructionSize;

        /// @brief  the stack. this is klutzy

        // this is the first time that i have ever used smart pointers. By default the stack will be 32-bit
        std::unique_ptr<StackBase> stack = std::make_unique<Stack<uint32_t>>();

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
        
        /// @brief Reset the debugger.
        static void Reset();

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
            uint32_t GetValue() { return AddrSpace::PeekU32(addr); }; 
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
