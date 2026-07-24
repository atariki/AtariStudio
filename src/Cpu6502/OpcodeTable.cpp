#include <AtariStudio/Cpu6502/OpcodeTable.h>

namespace atari::cpu6502
{

    namespace
    {

        constexpr InstructionInfo Illegal =
        {
            Instruction::Illegal,
            AddressMode::Implied,
            1,
            2,
            false,
            false
        };

    }

    const std::array<InstructionInfo, 256> OpcodeTable::m_table =
    {
        /*00*/
        OP(BRK,Implied,1,7,false),
        OP(ORA,IndexedIndirect,2,6,false),
        ILL,
        ILL,
        ILL,
        OP(ORA,ZeroPage,2,3,false),
        OP(ASL,ZeroPage,2,5,false),
        ILL,
        OP(PHP,Implied,1,3,false),
        OP(ORA,Immediate,2,2,false),
        OP(ASL,Accumulator,1,2,false),
        ILL,
        ILL,
        OP(ORA,Absolute,3,4,false),
        OP(ASL,Absolute,3,6,false),
        ILL,

        /*10*/
        OP(BPL,Relative,2,2,true),
        OP(ORA,IndirectIndexed,2,5,true),
        ILL,
        ILL,
        ILL,
        OP(ORA,ZeroPageX,2,4,false),
        OP(ASL,ZeroPageX,2,6,false),
        ILL,
        OP(CLC,Implied,1,2,false),
        OP(ORA,AbsoluteY,3,4,true),
        ILL,
        ILL,
        ILL,
        OP(ORA,AbsoluteX,3,4,true),
        OP(ASL,AbsoluteX,3,7,false),
        ILL,

        /*20*/
        OP(JSR,Absolute,3,6,false),
        OP(AND,IndexedIndirect,2,6,false),
        ILL,
        ILL,
        OP(BIT,ZeroPage,2,3,false),
        OP(AND,ZeroPage,2,3,false),
        OP(ROL,ZeroPage,2,5,false),
        ILL,
        OP(PLP,Implied,1,4,false),
        OP(AND,Immediate,2,2,false),
        OP(ROL,Accumulator,1,2,false),
        ILL,
        OP(BIT,Absolute,3,4,false),
        OP(AND,Absolute,3,4,false),
        OP(ROL,Absolute,3,6,false),
        ILL,

        /*30*/
        OP(BMI,Relative,2,2,true),
        OP(AND,IndirectIndexed,2,5,true),
        ILL,
        ILL,
        ILL,
        OP(AND,ZeroPageX,2,4,false),
        OP(ROL,ZeroPageX,2,6,false),
        ILL,
        OP(SEC,Implied,1,2,false),
        OP(AND,AbsoluteY,3,4,true),
        ILL,
        ILL,
        ILL,
        OP(AND,AbsoluteX,3,4,true),
        OP(ROL,AbsoluteX,3,7,false),
        ILL,

        /*40*/
OP(RTI,Implied,1,6,false),
OP(EOR,IndexedIndirect,2,6,false),
ILL,
ILL,
ILL,
OP(EOR,ZeroPage,2,3,false),
OP(LSR,ZeroPage,2,5,false),
ILL,
OP(PHA,Implied,1,3,false),
OP(EOR,Immediate,2,2,false),
OP(LSR,Accumulator,1,2,false),
ILL,
OP(JMP,Absolute,3,3,false),
OP(EOR,Absolute,3,4,false),
OP(LSR,Absolute,3,6,false),
ILL,

/*50*/
OP(BVC,Relative,2,2,true),
OP(EOR,IndirectIndexed,2,5,true),
ILL,
ILL,
ILL,
OP(EOR,ZeroPageX,2,4,false),
OP(LSR,ZeroPageX,2,6,false),
ILL,
OP(CLI,Implied,1,2,false),
OP(EOR,AbsoluteY,3,4,true),
ILL,
ILL,
ILL,
OP(EOR,AbsoluteX,3,4,true),
OP(LSR,AbsoluteX,3,7,false),
ILL,

/*60*/
OP(RTS,Implied,1,6,false),
OP(ADC,IndexedIndirect,2,6,false),
ILL,
ILL,
ILL,
OP(ADC,ZeroPage,2,3,false),
OP(ROR,ZeroPage,2,5,false),
ILL,
OP(PLA,Implied,1,4,false),
OP(ADC,Immediate,2,2,false),
OP(ROR,Accumulator,1,2,false),
ILL,
OP(JMP,Indirect,3,5,false),
OP(ADC,Absolute,3,4,false),
OP(ROR,Absolute,3,6,false),
ILL,

/*70*/
OP(BVS,Relative,2,2,true),
OP(ADC,IndirectIndexed,2,5,true),
ILL,
ILL,
ILL,
OP(ADC,ZeroPageX,2,4,false),
OP(ROR,ZeroPageX,2,6,false),
ILL,
OP(SEI,Implied,1,2,false),
OP(ADC,AbsoluteY,3,4,true),
ILL,
ILL,
ILL,
OP(ADC,AbsoluteX,3,4,true),
OP(ROR,AbsoluteX,3,7,false),
ILL,

/*80*/
ILL,
OP(STA, IndexedIndirect, 2, 6, false),
ILL,
ILL,
OP(STY, ZeroPage, 2, 3, false),
OP(STA, ZeroPage, 2, 3, false),
OP(STX, ZeroPage, 2, 3, false),
ILL,
OP(DEY, Implied, 1, 2, false),
ILL,
OP(TXA, Implied, 1, 2, false),
ILL,
OP(STY, Absolute, 3, 4, false),
OP(STA, Absolute, 3, 4, false),
OP(STX, Absolute, 3, 4, false),
ILL,

/*90*/
OP(BCC, Relative, 2, 2, true),
OP(STA, IndirectIndexed, 2, 6, false),
ILL,
ILL,
OP(STY, ZeroPageX, 2, 4, false),
OP(STA, ZeroPageX, 2, 4, false),
OP(STX, ZeroPageY, 2, 4, false),
ILL,
OP(TYA, Implied, 1, 2, false),
OP(STA, AbsoluteY, 3, 5, false),
OP(TXS, Implied, 1, 2, false),
ILL,
ILL,
OP(STA, AbsoluteX, 3, 5, false),
ILL,
ILL,

/*A0*/
OP(LDY, Immediate, 2, 2, false),
OP(LDA, IndexedIndirect, 2, 6, false),
OP(LDX, Immediate, 2, 2, false),
ILL,
OP(LDY, ZeroPage, 2, 3, false),
OP(LDA, ZeroPage, 2, 3, false),
OP(LDX, ZeroPage, 2, 3, false),
ILL,
OP(TAY, Implied, 1, 2, false),
OP(LDA, Immediate, 2, 2, false),
OP(TAX, Implied, 1, 2, false),
ILL,
OP(LDY, Absolute, 3, 4, false),
OP(LDA, Absolute, 3, 4, false),
OP(LDX, Absolute, 3, 4, false),
ILL,

/*B0*/
OP(BCS, Relative, 2, 2, true),
OP(LDA, IndirectIndexed, 2, 5, true),
ILL,
ILL,
OP(LDY, ZeroPageX, 2, 4, false),
OP(LDA, ZeroPageX, 2, 4, false),
OP(LDX, ZeroPageY, 2, 4, false),
ILL,
OP(CLV, Implied, 1, 2, false),
OP(LDA, AbsoluteY, 3, 4, true),
OP(TSX, Implied, 1, 2, false),
ILL,
OP(LDY, AbsoluteX, 3, 4, true),
OP(LDA, AbsoluteX, 3, 4, true),
OP(LDX, AbsoluteY, 3, 4, true),
ILL,

/*C0*/
OP(CPY, Immediate, 2, 2, false),
OP(CMP, IndexedIndirect, 2, 6, false),
ILL,
ILL,
OP(CPY, ZeroPage, 2, 3, false),
OP(CMP, ZeroPage, 2, 3, false),
OP(DEC, ZeroPage, 2, 5, false),
ILL,
OP(INY, Implied, 1, 2, false),
OP(CMP, Immediate, 2, 2, false),
OP(DEX, Implied, 1, 2, false),
ILL,
OP(CPY, Absolute, 3, 4, false),
OP(CMP, Absolute, 3, 4, false),
OP(DEC, Absolute, 3, 6, false),
ILL,

/*D0*/
OP(BNE, Relative, 2, 2, true),
OP(CMP, IndirectIndexed, 2, 5, true),
ILL,
ILL,
ILL,
OP(CMP, ZeroPageX, 2, 4, false),
OP(DEC, ZeroPageX, 2, 6, false),
ILL,
OP(CLD, Implied, 1, 2, false),
OP(CMP, AbsoluteY, 3, 4, true),
ILL,
ILL,
ILL,
OP(CMP, AbsoluteX, 3, 4, true),
OP(DEC, AbsoluteX, 3, 7, false),
ILL,

/*E0*/
OP(CPX, Immediate, 2, 2, false),
OP(SBC, IndexedIndirect, 2, 6, false),
ILL,
ILL,
OP(CPX, ZeroPage, 2, 3, false),
OP(SBC, ZeroPage, 2, 3, false),
OP(INC, ZeroPage, 2, 5, false),
ILL,
OP(INX, Implied, 1, 2, false),
OP(SBC, Immediate, 2, 2, false),
OP(NOP, Implied, 1, 2, false),
ILL,
OP(CPX, Absolute, 3, 4, false),
OP(SBC, Absolute, 3, 4, false),
OP(INC, Absolute, 3, 6, false),
ILL,

/*F0*/
OP(BEQ, Relative, 2, 2, true),
OP(SBC, IndirectIndexed, 2, 5, true),
ILL,
ILL,
ILL,
OP(SBC, ZeroPageX, 2, 4, false),
OP(INC, ZeroPageX, 2, 6, false),
ILL,
OP(SED, Implied, 1, 2, false),
OP(SBC, AbsoluteY, 3, 4, true),
ILL,
ILL,
ILL,
OP(SBC, AbsoluteX, 3, 4, true),
OP(INC, AbsoluteX, 3, 7, false),
ILL
};
    };

    const InstructionInfo& OpcodeTable::Get(Opcode opcode)
    {
        return m_table[opcode];
    }

} // namespace atari::cpu6502

namespace
{

    constexpr InstructionInfo Illegal =
    {
        Instruction::Illegal,
        AddressMode::Implied,
        1,
        2,
        false,
        false
    };

#define OP(i,m,l,c,p) \
InstructionInfo{Instruction::i,AddressMode::m,l,c,p,true}

#define ILL Illegal

}