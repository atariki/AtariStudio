#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Cpu6502/AddressMode.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/BranchConditionAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowGraph.h>
#include <AtariStudio/Disassembler/Disassembler.h>

namespace atari
{

// ============================================================
// Producer classification
// ============================================================

enum class FlagProducerKind
{
    //
    // Arithmetic, logical, load, transfer, increment,
    // decrement, shift, rotate, PLA, etc.
    //
    // The tested flag describes the resulting value.
    //
    ValueResult,

    //
    // CMP / CPX / CPY.
    //
    Compare,

    //
    // BIT.
    //
    BitTest,

    //
    // CLC / CLV.
    //
    ExplicitClear,

    //
    // SEC.
    //
    ExplicitSet,

    //
    // PLP / RTI.
    //
    // Processor status was restored from another source,
    // therefore the high-level value is not directly known.
    //
    StatusRestore,

    Unknown
};

// ============================================================
// Why producer search stopped
// ============================================================

enum class FlagProducerStopReason
{
    None,

    //
    // Reached routine entry without finding a producer.
    //
    EntryBoundary,

    //
    // More than one predecessor reaches the block.
    //
    // We cannot yet prove that every incoming path produces
    // the same flag value.
    //
    AmbiguousPredecessors,

    //
    // JSR may modify processor flags through the callee.
    //
    CallBarrier,

    //
    // A predecessor walk entered a CFG cycle.
    //
    Cycle,

    //
    // Referenced predecessor block was not found.
    //
    MissingBlock
};

// ============================================================
// Flag producer
// ============================================================

struct FlagProducer
{
    //
    // Branch that consumes the flag.
    //
    u16 branchAddress = 0;

    cpu6502::Instruction branchInstruction =
        cpu6502::Instruction::Illegal;

    ProcessorFlag flag =
        ProcessorFlag::Zero;

    //
    // Producer information.
    //
    bool found = false;

    u16 producerAddress = 0;

    cpu6502::Instruction producerInstruction =
        cpu6502::Instruction::Illegal;

    cpu6502::AddressMode producerAddressMode =
        cpu6502::AddressMode::Implied;

    std::array<u8, 3> producerBytes{};

    u8 producerLength = 0;

    FlagProducerKind kind =
        FlagProducerKind::Unknown;

    //
    // Used for CLC / SEC / CLV when the flag value is
    // statically known.
    //
    std::optional<FlagState> constantState;

    //
    // Number of instructions inspected while walking
    // backwards from the branch.
    //
    std::size_t instructionsBack = 0;

    //
    // Number of predecessor blocks crossed.
    //
    std::size_t blocksBack = 0;

    FlagProducerStopReason stopReason =
        FlagProducerStopReason::None;

    [[nodiscard]]
    bool IsLocal() const noexcept
    {
        return
            found &&
            blocksBack == 0;
    }

    [[nodiscard]]
    bool IsCrossBlock() const noexcept
    {
        return
            found &&
            blocksBack != 0;
    }

    [[nodiscard]]
    bool HasConstantState() const noexcept
    {
        return
            constantState.has_value();
    }
};

// ============================================================
// Per-routine analysis
// ============================================================

struct RoutineFlagProducerAnalysis
{
    u16 routineEntryAddress = 0;

    std::string routineName;

    std::vector<FlagProducer> producers;

    [[nodiscard]]
    const FlagProducer* FindBranch(
        u16 branchAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                producers.begin(),
                producers.end(),
                [branchAddress](
                    const FlagProducer& producer)
                {
                    return
                        producer.branchAddress ==
                        branchAddress;
                });

        if (iterator == producers.end())
        {
            return nullptr;
        }

        return &*iterator;
    }

    [[nodiscard]]
    std::size_t FoundCount() const noexcept
    {
        return static_cast<std::size_t>(
            std::count_if(
                producers.begin(),
                producers.end(),
                [](const FlagProducer& producer)
                {
                    return
                        producer.found;
                }));
    }

    [[nodiscard]]
    std::size_t UnresolvedCount() const noexcept
    {
        return
            producers.size() -
            FoundCount();
    }
};

// ============================================================
// Global analysis result
// ============================================================

struct FlagProducerAnalysisResult
{
    std::vector<RoutineFlagProducerAnalysis>
        routines;

    [[nodiscard]]
    const RoutineFlagProducerAnalysis* FindRoutine(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const RoutineFlagProducerAnalysis& routine)
                {
                    return
                        routine.routineEntryAddress ==
                        entryAddress;
                });

        if (iterator == routines.end())
        {
            return nullptr;
        }

        return &*iterator;
    }

    [[nodiscard]]
    const FlagProducer* Find(
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
    std::size_t BranchCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.producers.size();
        }

        return count;
    }

    [[nodiscard]]
    std::size_t FoundCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& routine :
             routines)
        {
            count +=
                routine.FoundCount();
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

class FlagProducerAnalyzer
{
public:

    [[nodiscard]]
    FlagProducerAnalysisResult Analyze(
        Project& project,
        const ControlFlowGraphAnalysisResult& graphs) const
    {
        FlagProducerAnalysisResult result;

        Disassembler disassembler;

        for (const auto& graph :
             graphs.routines)
        {
            RoutineFlagProducerAnalysis
                routineResult;

            routineResult.routineEntryAddress =
                graph.routineEntryAddress;

            routineResult.routineName =
                graph.routineName;

            for (const auto& node :
                 graph.nodes)
            {
                AnalyzeNode(
                    project,
                    disassembler,
                    graph,
                    node,
                    routineResult);
            }

            std::sort(
                routineResult.producers.begin(),
                routineResult.producers.end(),
                [](const FlagProducer& left,
                   const FlagProducer& right)
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

    enum class ScanResult
    {
        Continue,
        Found,
        CallBarrier
    };

    // ========================================================
    // Node analysis
    // ========================================================

    static void AnalyzeNode(
        Project& project,
        const Disassembler& disassembler,
        const RoutineControlFlowGraph& graph,
        const ControlFlowGraphNode& node,
        RoutineFlagProducerAnalysis& routineResult)
    {
        if (node.instructionAddresses.empty())
        {
            return;
        }

        //
        // Conditional branches terminate basic blocks,
        // therefore the last instruction is the branch
        // consuming the processor flag.
        //
        const u16 branchAddress =
            node.instructionAddresses.back();

        const auto branch =
            disassembler.Decode(
                project.GetMemory(),
                branchAddress);

        ProcessorFlag flag =
            ProcessorFlag::Zero;

        if (!DecodeBranchFlag(
                branch.instruction,
                flag))
        {
            return;
        }

        FlagProducer producer;

        producer.branchAddress =
            branchAddress;

        producer.branchInstruction =
            branch.instruction;

        producer.flag =
            flag;

        FindProducer(
            project,
            disassembler,
            graph,
            node,
            producer);

        routineResult.producers.push_back(
            std::move(
                producer));
    }

    // ========================================================
    // Backward producer search
    // ========================================================

    static void FindProducer(
        Project& project,
        const Disassembler& disassembler,
        const RoutineControlFlowGraph& graph,
        const ControlFlowGraphNode& branchNode,
        FlagProducer& result)
    {
        const ControlFlowGraphNode* currentNode =
            &branchNode;

        u16 stopBeforeAddress =
            result.branchAddress;

        bool firstBlock =
            true;

        std::vector<u16> visitedBlocks;

        while (currentNode != nullptr)
        {
            if (std::find(
                    visitedBlocks.begin(),
                    visitedBlocks.end(),
                    currentNode->address) !=
                visitedBlocks.end())
            {
                result.stopReason =
                    FlagProducerStopReason::Cycle;

                return;
            }

            visitedBlocks.push_back(
                currentNode->address);

            const ScanResult scanResult =
                ScanBlockBackward(
                    project,
                    disassembler,
                    *currentNode,
                    result.flag,
                    firstBlock
                        ? std::optional<u16>(
                              stopBeforeAddress)
                        : std::nullopt,
                    result);

            if (scanResult ==
                ScanResult::Found)
            {
                result.stopReason =
                    FlagProducerStopReason::None;

                return;
            }

            if (scanResult ==
                ScanResult::CallBarrier)
            {
                result.stopReason =
                    FlagProducerStopReason::CallBarrier;

                return;
            }

            if (currentNode->predecessors.empty())
            {
                result.stopReason =
                    FlagProducerStopReason::EntryBoundary;

                return;
            }

            if (currentNode->predecessors.size() != 1)
            {
                result.stopReason =
                    FlagProducerStopReason::
                        AmbiguousPredecessors;

                return;
            }

            const u16 predecessorAddress =
                currentNode->predecessors.front();

            currentNode =
                graph.FindNode(
                    predecessorAddress);

            if (currentNode == nullptr)
            {
                result.stopReason =
                    FlagProducerStopReason::MissingBlock;

                return;
            }

            ++result.blocksBack;

            firstBlock =
                false;
        }

        result.stopReason =
            FlagProducerStopReason::MissingBlock;
    }

    // ========================================================
    // Scan one basic block backwards
    // ========================================================

    [[nodiscard]]
    static ScanResult ScanBlockBackward(
        Project& project,
        const Disassembler& disassembler,
        const ControlFlowGraphNode& node,
        ProcessorFlag flag,
        const std::optional<u16>& stopBeforeAddress,
        FlagProducer& result)
    {
        bool mayScan =
            !stopBeforeAddress.has_value();

        for (auto iterator =
                 node.instructionAddresses.rbegin();
             iterator !=
                 node.instructionAddresses.rend();
             ++iterator)
        {
            const u16 address =
                *iterator;

            //
            // For the first block, ignore the branch itself
            // and begin with the instruction before it.
            //
            if (!mayScan)
            {
                if (address ==
                    stopBeforeAddress.value())
                {
                    mayScan =
                        true;
                }

                continue;
            }

            ++result.instructionsBack;

            const auto instruction =
                disassembler.Decode(
                    project.GetMemory(),
                    address);

            const FlagProducerKind kind =
                ClassifyProducer(
                    instruction.instruction,
                    flag);

            if (kind !=
                FlagProducerKind::Unknown)
            {
                FillProducer(
                    instruction,
                    flag,
                    kind,
                    result);

                return
                    ScanResult::Found;
            }

            //
            // A subroutine can change N/V/Z/C.
            //
            // Until interprocedural flag summaries are
            // implemented, do not search through JSR.
            //
            if (instruction.instruction ==
                cpu6502::Instruction::JSR)
            {
                return
                    ScanResult::CallBarrier;
            }
        }

        return
            ScanResult::Continue;
    }

    // ========================================================
    // Store producer information
    // ========================================================

    static void FillProducer(
        const DisassembledInstruction& instruction,
        ProcessorFlag flag,
        FlagProducerKind kind,
        FlagProducer& result)
    {
        result.found =
            true;

        result.producerAddress =
            instruction.address;

        result.producerInstruction =
            instruction.instruction;

        result.producerAddressMode =
            instruction.addressMode;

        result.producerBytes =
            instruction.bytes;

        result.producerLength =
            instruction.length;

        result.kind =
            kind;

        result.constantState.reset();

        //
        // Some status instructions produce a known
        // constant state.
        //
        if (flag ==
                ProcessorFlag::Carry &&
            instruction.instruction ==
                cpu6502::Instruction::CLC)
        {
            result.constantState =
                FlagState::Clear;
        }
        else if (
            flag ==
                ProcessorFlag::Carry &&
            instruction.instruction ==
                cpu6502::Instruction::SEC)
        {
            result.constantState =
                FlagState::Set;
        }
        else if (
            flag ==
                ProcessorFlag::Overflow &&
            instruction.instruction ==
                cpu6502::Instruction::CLV)
        {
            result.constantState =
                FlagState::Clear;
        }
    }

    // ========================================================
    // Branch -> consumed flag
    // ========================================================

    [[nodiscard]]
    static bool DecodeBranchFlag(
        cpu6502::Instruction instruction,
        ProcessorFlag& flag) noexcept
    {
        using cpu6502::Instruction;

        switch (instruction)
        {
        case Instruction::BCC:
        case Instruction::BCS:

            flag =
                ProcessorFlag::Carry;

            return true;

        case Instruction::BEQ:
        case Instruction::BNE:

            flag =
                ProcessorFlag::Zero;

            return true;

        case Instruction::BMI:
        case Instruction::BPL:

            flag =
                ProcessorFlag::Negative;

            return true;

        case Instruction::BVC:
        case Instruction::BVS:

            flag =
                ProcessorFlag::Overflow;

            return true;

        default:

            return false;
        }
    }

    // ========================================================
    // Producer classification
    // ========================================================

    [[nodiscard]]
    static FlagProducerKind ClassifyProducer(
        cpu6502::Instruction instruction,
        ProcessorFlag flag) noexcept
    {
        using cpu6502::Instruction;

        //
        // ----------------------------------------------------
        // Processor status restore.
        // ----------------------------------------------------
        //
        if (instruction ==
                Instruction::PLP ||
            instruction ==
                Instruction::RTI)
        {
            return
                FlagProducerKind::StatusRestore;
        }

        //
        // ----------------------------------------------------
        // Carry.
        // ----------------------------------------------------
        //
        if (flag ==
            ProcessorFlag::Carry)
        {
            switch (instruction)
            {
            case Instruction::CLC:

                return
                    FlagProducerKind::ExplicitClear;

            case Instruction::SEC:

                return
                    FlagProducerKind::ExplicitSet;

            case Instruction::CMP:
            case Instruction::CPX:
            case Instruction::CPY:

                return
                    FlagProducerKind::Compare;

            case Instruction::ADC:
            case Instruction::ASL:
            case Instruction::LSR:
            case Instruction::ROL:
            case Instruction::ROR:
            case Instruction::SBC:

                return
                    FlagProducerKind::ValueResult;

            default:

                return
                    FlagProducerKind::Unknown;
            }
        }

        //
        // ----------------------------------------------------
        // Overflow.
        // ----------------------------------------------------
        //
        if (flag ==
            ProcessorFlag::Overflow)
        {
            switch (instruction)
            {
            case Instruction::CLV:

                return
                    FlagProducerKind::ExplicitClear;

            case Instruction::BIT:

                return
                    FlagProducerKind::BitTest;

            case Instruction::ADC:
            case Instruction::SBC:

                return
                    FlagProducerKind::ValueResult;

            default:

                return
                    FlagProducerKind::Unknown;
            }
        }

        //
        // ----------------------------------------------------
        // Zero.
        // ----------------------------------------------------
        //
        if (flag ==
            ProcessorFlag::Zero)
        {
            switch (instruction)
            {
            case Instruction::CMP:
            case Instruction::CPX:
            case Instruction::CPY:

                return
                    FlagProducerKind::Compare;

            case Instruction::BIT:

                return
                    FlagProducerKind::BitTest;

            case Instruction::ADC:
            case Instruction::AND:
            case Instruction::ASL:
            case Instruction::DEC:
            case Instruction::DEX:
            case Instruction::DEY:
            case Instruction::EOR:
            case Instruction::INC:
            case Instruction::INX:
            case Instruction::INY:
            case Instruction::LDA:
            case Instruction::LDX:
            case Instruction::LDY:
            case Instruction::LSR:
            case Instruction::ORA:
            case Instruction::PLA:
            case Instruction::ROL:
            case Instruction::ROR:
            case Instruction::SBC:
            case Instruction::TAX:
            case Instruction::TAY:
            case Instruction::TSX:
            case Instruction::TXA:
            case Instruction::TYA:

                return
                    FlagProducerKind::ValueResult;

            default:

                return
                    FlagProducerKind::Unknown;
            }
        }

        //
        // ----------------------------------------------------
        // Negative.
        // ----------------------------------------------------
        //
        if (flag ==
            ProcessorFlag::Negative)
        {
            switch (instruction)
            {
            case Instruction::CMP:
            case Instruction::CPX:
            case Instruction::CPY:

                return
                    FlagProducerKind::Compare;

            case Instruction::BIT:

                return
                    FlagProducerKind::BitTest;

            case Instruction::ADC:
            case Instruction::AND:
            case Instruction::ASL:
            case Instruction::DEC:
            case Instruction::DEX:
            case Instruction::DEY:
            case Instruction::EOR:
            case Instruction::INC:
            case Instruction::INX:
            case Instruction::INY:
            case Instruction::LDA:
            case Instruction::LDX:
            case Instruction::LDY:
            case Instruction::LSR:
            case Instruction::ORA:
            case Instruction::PLA:
            case Instruction::ROL:
            case Instruction::ROR:
            case Instruction::SBC:
            case Instruction::TAX:
            case Instruction::TAY:
            case Instruction::TSX:
            case Instruction::TXA:
            case Instruction::TYA:

                return
                    FlagProducerKind::ValueResult;

            default:

                return
                    FlagProducerKind::Unknown;
            }
        }

        return
            FlagProducerKind::Unknown;
    }
};

} // namespace atari