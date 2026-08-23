#pragma once 

#include <Motion.hpp>
#include <component/component.hpp>

namespace Motion
{
    /// @brief Base class for components that implement a CPU
    class ComponentCPU : public Component
    {
    public:
        /// @brief get the name of this component. immutable const char*.
        const char* GetName() { return "CPU Generic Base Class (error)"; };

        /// @brief returns a boolean indicating if this cpu is in privileged mode. most cpus only have two levels of privilege and x86 has 4, but 2 are almost never used.
        /// @return a boolean indicating if the cpu is in privileged mode
        virtual bool IsPrivilegedMode() { return true; };

        /// @brief Where the CPU is currently executing. Devices use this to say who touched them.
        virtual uint32_t GetProgramCounter() { return 0; };

        // Getters for private fields

        /// @brief returns a boolean indicating if this cpu is in reset.
        /// @return a boolean indicating if this cpu is in reset.
        bool GetIsInReset() { return isInReset; };

        // Setters for private fields
        void SetIsInReset(bool inReset) { this->isInReset = inReset; };

        /// @brief assert an irq line
        /// @param irqNum the irq number to fire
        virtual void SetIRQLine(int32_t irqNum) { };


    protected: 
        bool isInReset;
    };
}