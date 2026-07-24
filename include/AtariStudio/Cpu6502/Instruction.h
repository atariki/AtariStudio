#pragma once

#include <cstdint>

#include <AtariStudio/Cpu6502/AddressMode.h>

namespace atari::cpu6502
{

    enum class Instruction : std::uint8_t
    {
        ADC,
        AND,
        ASL,

        BCC,
        BCS,
        BEQ,
        BIT,
        BMI,
        BNE,
        BPL,
        BRK,
        BVC,
        BVS,

        CLC,
        CLD,
        CLI,
        CLV,

        CMP,
        CPX,
        CPY,

        DEC,
        DEX,
        DEY,

        EOR,

        INC,
        INX,
        INY,

        JMP,
        JSR,

        LDA,
        LDX,
        LDY,

        LSR,

        NOP,

        ORA,

        PHA,
        PHP,
        PLA,
        PLP,

        ROL,
        ROR,
        RTI,
        RTS,

        SBC,

        SEC,
        SED,
        SEI,

        STA,
        STX,
        STY,

        TAX,
        TAY,
        TSX,
        TXA,
        TXS,
        TYA,

        Illegal
    };

    struct InstructionInfo
    {
        Instruction instruction;

        AddressMode addressMode;

        std::uint8_t length;

        std::uint8_t cycles;

        bool pageCrossCycle;

        bool official;
    };

} // namespace atari::cpu6502