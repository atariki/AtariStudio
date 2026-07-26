#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Cpu6502/AddressMode.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/BranchConditionAnalyzer.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Disassembler/FlagProducerAnalyzer.h>

namespace atari
{

// ============================================================
// Semantic condition kind
// ============================================================

enum class SemanticConditionKind
{
    //
    // Example:
    //
    //     INX
    //     BNE ...
    //
    // becomes:
    //
    //     X != 0
    //
    ZeroTest,

    //
    // Example:
    //
    //     TXA
    //     BPL ...
    //
    // becomes:
    //
    //     (X & $80) == 0
    //
    SignTest,

    //
    // Example:
    //
    //     CPY #$03
    //     BNE ...
    //
    // becomes:
    //
    //     Y != $03
    //
    Compare,

    //
    // Example:
    //
    //     BIT $D01F
    //     BEQ ...
    //
    // becomes:
    //
    //     (A & mem[$D01F]) == 0
    //
    BitTest,

    //
    // CLC / SEC / CLV.
    //
    Constant,

    //
    // Producer exists, but we cannot yet safely lift
    // its meaning to a high-level expression.
    //
    RawFlag,

    //
    // Producer itself could not be found.
    //
    Unresolved
};

// ============================================================
// One semantic branch condition
// ============================================================

struct SemanticCondition
{
    u16 branchAddress = 0;

    cpu6502::Instruction branchInstruction =
        cpu6502::Instruction::Illegal;

    ProcessorFlag flag =
        ProcessorFlag::Zero;

    FlagState branchTakenState =
        FlagState::Clear;

    FlagState fallthroughState =
        FlagState::Set;

    bool producerFound = false;

    u16 producerAddress = 0;

    cpu6502::Instruction producerInstruction =
        cpu6502::Instruction::Illegal;

    FlagProducerKind producerKind =
        FlagProducerKind::Unknown;

    SemanticConditionKind kind =
        SemanticConditionKind::Unresolved;

    //
    // True when the expression no longer directly refers
    // to C/Z/N/V and instead describes the value that
    // produced the tested flag.
    //
    bool semanticResolved = false;

    //
    // Expression when the conditional branch is taken.
    //
    std::string branchTakenExpression;

    //
    // Expression when execution falls through.
    //
    std::string fallthroughExpression;

    [[nodiscard]]
    const std::string& ExpressionForState(
        FlagState state) const noexcept
    {
        if (state ==
            branchTakenState)
        {
            return
                branchTakenExpression;
        }

        return
            fallthroughExpression;
    }
};

// ============================================================
// Per-routine result
// ============================================================

struct RoutineSemanticConditionAnalysis
{
    u16 routineEntryAddress = 0;

    std::string routineName;

    std::vector<SemanticCondition> conditions;

    [[nodiscard]]
    const SemanticCondition* FindBranch(
        u16 branchAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                conditions.begin(),
                conditions.end(),
                [branchAddress](
                    const SemanticCondition& condition)
                {
                    return
                        condition.branchAddress ==
                        branchAddress;
                });

        if (iterator ==
            conditions.end())
        {
            return nullptr;
        }

        return
            &*iterator;
    }

    [[nodiscard]]
    std::size_t ResolvedCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                conditions.begin(),
                conditions.end(),
                [](const SemanticCondition& condition)
                {
                    return
                        condition.semanticResolved;
                }));
    }

    [[nodiscard]]
    std::size_t UnresolvedCount() const noexcept
    {
        return
            conditions.size() -
            ResolvedCount();
    }
};

// ============================================================
// Global result
// ============================================================

struct SemanticConditionAnalysisResult
{
    std::vector<RoutineSemanticConditionAnalysis>
        routines;

    [[nodiscard]]
    const RoutineSemanticConditionAnalysis* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutineSemanticConditionAnalysis& routine)
                {
                    return
                        routine.routineEntryAddress ==
                        entryAddress;
                });

        if (iterator ==
            routines.end())
        {
            return nullptr;
        }

        return
            &*iterator;
    }

    [[nodiscard]]
    const SemanticCondition* Find(
        u16 routineEntryAddress,
        u16 branchAddress) const noexcept
    {
        const auto* routine =
            FindRoutine(
                routineEntryAddress);

        if (routine == nullptr)
        {
            return nullptr;
        }

        return
            routine->FindBranch(
                branchAddress);
    }

    [[nodiscard]]
    std::size_t ConditionCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.conditions.size();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t ResolvedCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.ResolvedCount();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t UnresolvedCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.UnresolvedCount();
        }

        return count;
    }
};

// ============================================================
// Analyzer
// ============================================================

class SemanticConditionAnalyzer
{
public:

    [[nodiscard]]
    SemanticConditionAnalysisResult Analyze(
        Project& project,
        const FlagProducerAnalysisResult& flagProducers) const
    {
        SemanticConditionAnalysisResult result;

        Disassembler disassembler;

        for (const auto& routine :
             flagProducers.routines)
        {
            RoutineSemanticConditionAnalysis
                routineResult;

            routineResult.routineEntryAddress =
                routine.routineEntryAddress;

            routineResult.routineName =
                routine.routineName;

            for (const auto& producer :
                 routine.producers)
            {
                SemanticCondition
                    condition;

                BuildCondition(
                    project,
                    disassembler,
                    producer,
                    condition);

                routineResult.conditions.push_back(
                    std::move(
                        condition));
            }

            std::sort(
                routineResult.conditions.begin(),
                routineResult.conditions.end(),
                [](const SemanticCondition& left,
                   const SemanticCondition& right)
                {
                    return
                        left.branchAddress <
                        right.branchAddress;
                });

            result.routines.push_back(
                std::move(
                    routineResult));
        }

        return result;
    }

private:

    // ========================================================
    // Build complete semantic condition
    // ========================================================

    static void BuildCondition(
        Project& project,
        const Disassembler& disassembler,
        const FlagProducer& producer,
        SemanticCondition& result)
    {
        result.branchAddress =
            producer.branchAddress;

        result.branchInstruction =
            producer.branchInstruction;

        result.flag =
            producer.flag;

        result.producerFound =
            producer.found;

        result.producerAddress =
            producer.producerAddress;

        result.producerInstruction =
            producer.producerInstruction;

        result.producerKind =
            producer.kind;

        DecodeBranchStates(
            producer.branchInstruction,
            result.branchTakenState,
            result.fallthroughState);

        //
        // No producer:
        //
        // preserve the raw processor flag expression.
        //
        if (!producer.found)
        {
            result.kind =
                SemanticConditionKind::Unresolved;

            result.semanticResolved =
                false;

            result.branchTakenExpression =
                RawFlagExpression(
                    producer.flag,
                    result.branchTakenState);

            result.fallthroughExpression =
                RawFlagExpression(
                    producer.flag,
                    result.fallthroughState);

            return;
        }

        const auto decoded =
            disassembler.Decode(
                project.GetMemory(),
                producer.producerAddress);

        SemanticConditionKind semanticKind =
            SemanticConditionKind::RawFlag;

        bool resolved =
            false;

        result.branchTakenExpression =
            BuildExpression(
                decoded,
                producer,
                result.branchTakenState,
                semanticKind,
                resolved);

        result.kind =
            semanticKind;

        result.semanticResolved =
            resolved;

        SemanticConditionKind inverseKind =
            SemanticConditionKind::RawFlag;

        bool inverseResolved =
            false;

        result.fallthroughExpression =
            BuildExpression(
                decoded,
                producer,
                result.fallthroughState,
                inverseKind,
                inverseResolved);

        //
        // Both sides should normally have the same semantic
        // quality. Be conservative if one side could not
        // be reconstructed.
        //
        result.semanticResolved =
            result.semanticResolved &&
            inverseResolved;
    }

    // ========================================================
    // Build expression for one required flag state
    // ========================================================

    [[nodiscard]]
    static std::string BuildExpression(
        const DisassembledInstruction& instruction,
        const FlagProducer& producer,
        FlagState requiredState,
        SemanticConditionKind& kind,
        bool& resolved)
    {
        //
        // ----------------------------------------------------
        // Constant flag producer.
        // ----------------------------------------------------
        //

        if (producer.constantState.has_value())
        {
            kind =
                SemanticConditionKind::Constant;

            resolved =
                true;

            return
                producer.constantState.value() ==
                        requiredState
                    ? "true"
                    : "false";
        }

        //
        // ----------------------------------------------------
        // CMP / CPX / CPY
        // ----------------------------------------------------
        //

        if (producer.kind ==
            FlagProducerKind::Compare)
        {
            const std::string left =
                CompareLeftExpression(
                    instruction.instruction);

            const std::string right =
                OperandValueExpression(
                    instruction);

            if (!left.empty() &&
                !right.empty())
            {
                //
                // Z from CMP/CPX/CPY:
                //
                //     Z=1 -> equal
                //     Z=0 -> not equal
                //
                if (producer.flag ==
                    ProcessorFlag::Zero)
                {
                    kind =
                        SemanticConditionKind::Compare;

                    resolved =
                        true;

                    return
                        left +
                        (requiredState ==
                                 FlagState::Set
                             ? " == "
                             : " != ") +
                        right;
                }

                //
                // C from CMP/CPX/CPY performs an unsigned
                // comparison:
                //
                //     C=1 -> register >= operand
                //     C=0 -> register < operand
                //
                if (producer.flag ==
                    ProcessorFlag::Carry)
                {
                    kind =
                        SemanticConditionKind::Compare;

                    resolved =
                        true;

                    return
                        left +
                        (requiredState ==
                                 FlagState::Set
                             ? " >= "
                             : " < ") +
                        right;
                }

                //
                // N after CMP is the high bit of the
                // subtraction result. It is NOT a generally
                // safe signed less-than comparison because
                // CMP does not provide signed-overflow
                // correction.
                //
                // Keep it as raw N for now.
                //
            }
        }

        //
        // ----------------------------------------------------
        // BIT
        // ----------------------------------------------------
        //

        if (producer.kind ==
            FlagProducerKind::BitTest)
        {
            const std::string operand =
                OperandValueExpression(
                    instruction);

            if (!operand.empty())
            {
                if (producer.flag ==
                    ProcessorFlag::Zero)
                {
                    kind =
                        SemanticConditionKind::BitTest;

                    resolved =
                        true;

                    //
                    // BIT:
                    //
                    //     Z = (A & operand) == 0
                    //
                    return
                        "(A & " +
                        operand +
                        ")" +
                        (requiredState ==
                                 FlagState::Set
                             ? " == 0"
                             : " != 0");
                }

                if (producer.flag ==
                    ProcessorFlag::Negative)
                {
                    kind =
                        SemanticConditionKind::BitTest;

                    resolved =
                        true;

                    //
                    // N receives bit 7 of the operand.
                    //
                    return
                        "(" +
                        operand +
                        " & $80)" +
                        (requiredState ==
                                 FlagState::Set
                             ? " != 0"
                             : " == 0");
                }

                if (producer.flag ==
                    ProcessorFlag::Overflow)
                {
                    kind =
                        SemanticConditionKind::BitTest;

                    resolved =
                        true;

                    //
                    // V receives bit 6 of the operand.
                    //
                    return
                        "(" +
                        operand +
                        " & $40)" +
                        (requiredState ==
                                 FlagState::Set
                             ? " != 0"
                             : " == 0");
                }
            }
        }

        //
        // ----------------------------------------------------
        // General result-producing instruction.
        // ----------------------------------------------------
        //

        if (producer.kind ==
            FlagProducerKind::ValueResult)
        {
            const std::string value =
                ResultValueExpression(
                    instruction);

            if (!value.empty())
            {
                //
                // Zero flag:
                //
                //     Z=1 -> result == 0
                //     Z=0 -> result != 0
                //
                if (producer.flag ==
                    ProcessorFlag::Zero)
                {
                    kind =
                        SemanticConditionKind::ZeroTest;

                    resolved =
                        true;

                    return
                        value +
                        (requiredState ==
                                 FlagState::Set
                             ? " == 0"
                             : " != 0");
                }

                //
                // Negative flag:
                //
                //     N = bit 7 of result
                //
                if (producer.flag ==
                    ProcessorFlag::Negative)
                {
                    kind =
                        SemanticConditionKind::SignTest;

                    resolved =
                        true;

                    return
                        "(" +
                        value +
                        " & $80)" +
                        (requiredState ==
                                 FlagState::Set
                             ? " != 0"
                             : " == 0");
                }
            }
        }

        //
        // ----------------------------------------------------
        // Anything not safely lifted remains a raw
        // processor flag expression.
        // ----------------------------------------------------
        //

        kind =
            SemanticConditionKind::RawFlag;

        resolved =
            false;

        return
            RawFlagExpression(
                producer.flag,
                requiredState);
    }

    // ========================================================
    // Expression representing the value whose Z/N flags
    // were produced.
    // ========================================================

    [[nodiscard]]
    static std::string ResultValueExpression(
        const DisassembledInstruction& instruction)
    {
        using cpu6502::Instruction;

        switch (instruction.instruction)
        {
        //
        // Loads.
        //
        // Since the branch immediately consumes Z/N,
        // expressing the original loaded value is more
        // informative than merely writing A/X/Y.
        //
        case Instruction::LDA:
        case Instruction::LDX:
        case Instruction::LDY:

            return
                OperandValueExpression(
                    instruction);

        //
        // X register modification.
        //
        case Instruction::DEX:
        case Instruction::INX:

            return "X";

        //
        // Y register modification.
        //
        case Instruction::DEY:
        case Instruction::INY:

            return "Y";

        //
        // Transfers.
        //
        // The flags describe the transferred source value.
        //
        case Instruction::TAX:
        case Instruction::TAY:

            return "A";

        case Instruction::TXA:

            return "X";

        case Instruction::TYA:

            return "Y";

        case Instruction::TSX:

            return "SP";

        //
        // Accumulator result.
        //
        case Instruction::ADC:
        case Instruction::AND:
        case Instruction::EOR:
        case Instruction::ORA:
        case Instruction::PLA:
        case Instruction::SBC:

            return "A";

        //
        // Memory increment/decrement.
        //
        case Instruction::INC:
        case Instruction::DEC:

            return
                MemoryExpression(
                    instruction);

        //
        // Shift / rotate can operate on either A or memory.
        //
        case Instruction::ASL:
        case Instruction::LSR:
        case Instruction::ROL:
        case Instruction::ROR:

            if (instruction.addressMode ==
                cpu6502::AddressMode::Accumulator)
            {
                return "A";
            }

            return
                MemoryExpression(
                    instruction);

        default:

            return {};
        }
    }

    // ========================================================
    // Compare expressions
    // ========================================================

    [[nodiscard]]
    static std::string CompareLeftExpression(
        cpu6502::Instruction instruction)
    {
        using cpu6502::Instruction;

        switch (instruction)
        {
        case Instruction::CMP:
            return "A";

        case Instruction::CPX:
            return "X";

        case Instruction::CPY:
            return "Y";

        default:
            return {};
        }
    }

    // ========================================================
    // Operand as a value
    // ========================================================

    [[nodiscard]]
    static std::string OperandValueExpression(
        const DisassembledInstruction& instruction)
    {
        using cpu6502::AddressMode;

        switch (instruction.addressMode)
        {
        case AddressMode::Immediate:

            return
                RemoveImmediatePrefix(
                    instruction.operand);

        case AddressMode::Accumulator:

            return "A";

        case AddressMode::Implied:

            return {};

        default:

            return
                MemoryExpression(
                    instruction);
        }
    }

    // ========================================================
    // Memory expression
    // ========================================================

    [[nodiscard]]
    static std::string MemoryExpression(
        const DisassembledInstruction& instruction)
    {
        if (instruction.operand.empty())
        {
            return {};
        }

        return
            "mem[" +
            instruction.operand +
            "]";
    }

    // ========================================================
    // Remove '#' from an immediate operand
    // ========================================================

    [[nodiscard]]
    static std::string RemoveImmediatePrefix(
        const std::string& operand)
    {
        if (!operand.empty() &&
            operand.front() == '#')
        {
            return
                operand.substr(1);
        }

        return
            operand;
    }

    // ========================================================
    // Raw processor flag fallback
    // ========================================================

    [[nodiscard]]
    static std::string RawFlagExpression(
        ProcessorFlag flag,
        FlagState state)
    {
        std::string expression =
            FlagName(
                flag);

        expression +=
            state ==
                    FlagState::Set
                ? " == 1"
                : " == 0";

        return
            expression;
    }

    [[nodiscard]]
    static const char* FlagName(
        ProcessorFlag flag) noexcept
    {
        switch (flag)
        {
        case ProcessorFlag::Carry:
            return "C";

        case ProcessorFlag::Zero:
            return "Z";

        case ProcessorFlag::Negative:
            return "N";

        case ProcessorFlag::Overflow:
            return "V";

        default:
            return "?";
        }
    }

    // ========================================================
    // Conditional branch semantics
    // ========================================================

    static void DecodeBranchStates(
        cpu6502::Instruction instruction,
        FlagState& takenState,
        FlagState& fallthroughState) noexcept
    {
        using cpu6502::Instruction;

        switch (instruction)
        {
        case Instruction::BCC:

            takenState =
                FlagState::Clear;

            fallthroughState =
                FlagState::Set;

            break;

        case Instruction::BCS:

            takenState =
                FlagState::Set;

            fallthroughState =
                FlagState::Clear;

            break;

        case Instruction::BEQ:

            takenState =
                FlagState::Set;

            fallthroughState =
                FlagState::Clear;

            break;

        case Instruction::BNE:

            takenState =
                FlagState::Clear;

            fallthroughState =
                FlagState::Set;

            break;

        case Instruction::BMI:

            takenState =
                FlagState::Set;

            fallthroughState =
                FlagState::Clear;

            break;

        case Instruction::BPL:

            takenState =
                FlagState::Clear;

            fallthroughState =
                FlagState::Set;

            break;

        case Instruction::BVC:

            takenState =
                FlagState::Clear;

            fallthroughState =
                FlagState::Set;

            break;

        case Instruction::BVS:

            takenState =
                FlagState::Set;

            fallthroughState =
                FlagState::Clear;

            break;

        default:

            takenState =
                FlagState::Clear;

            fallthroughState =
                FlagState::Set;

            break;
        }
    }
};

} // namespace atari