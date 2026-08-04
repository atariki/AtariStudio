#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/BasicBlockAnalyzer.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Disassembler/DisassemblyMetadata.h>
#include <AtariStudio/Disassembler/LoopStructureAnalyzer.h>
#include <AtariStudio/Disassembler/SemanticConditionAnalyzer.h>
#include <AtariStudio/Disassembler/StructuredControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/StructuredStatementFormatter.h>

namespace atari
{

enum class StructuredExpressionKind
{
    Empty,
    Block,
    Statement,
    If,
    IfElse,
    While,
    DoWhile,
    InfiniteLoop,
    Break,
    Continue
};

struct StructuredExpression
{
    StructuredExpressionKind kind =
        StructuredExpressionKind::Empty;

    u16 address = 0;

    std::string condition;
    std::string statement;

    std::vector<StructuredExpression> children;
    std::vector<StructuredExpression> elseChildren;
};

struct StructuredExpressionResult
{
    std::vector<StructuredExpression> roots;

    [[nodiscard]]
    std::size_t RootCount() const noexcept
    {
        return roots.size();
    }

    [[nodiscard]]
    std::size_t ExpressionCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& root : roots)
        {
            count += CountExpressions(root);
        }

        return count;
    }

    [[nodiscard]]
    std::size_t StatementCount() const noexcept
    {
        std::size_t count = 0;

        for (const auto& root : roots)
        {
            count +=
                CountKind(
                    root,
                    StructuredExpressionKind::Statement);
        }

        return count;
    }

private:

    [[nodiscard]]
    static std::size_t CountExpressions(
        const StructuredExpression& expression) noexcept
    {
        std::size_t count =
            expression.kind == StructuredExpressionKind::Block ||
            expression.kind == StructuredExpressionKind::Empty
                ? 0
                : 1;

        for (const auto& child : expression.children)
        {
            count += CountExpressions(child);
        }

        for (const auto& child : expression.elseChildren)
        {
            count += CountExpressions(child);
        }

        return count;
    }

    [[nodiscard]]
    static std::size_t CountKind(
        const StructuredExpression& expression,
        StructuredExpressionKind kind) noexcept
    {
        std::size_t count =
            expression.kind == kind
                ? 1
                : 0;

        for (const auto& child : expression.children)
        {
            count +=
                CountKind(
                    child,
                    kind);
        }

        for (const auto& child : expression.elseChildren)
        {
            count +=
                CountKind(
                    child,
                    kind);
        }

        return count;
    }

};

class StructuredExpressionBuilder
{
public:

    [[nodiscard]]
    StructuredExpressionResult Build(
        const Project& project,
        const BasicBlockAnalysisResult& basicBlocks,
        const DisassemblyMetadata& metadata,
        const StructuredControlFlowAnalysisResult& structuredFlow,
        const SemanticConditionAnalysisResult& semanticConditions,
        const LoopStructureAnalysisResult& loopStructures) const
    {
        StructuredExpressionResult result;

        std::vector<u16> routineAddresses =
            CollectRoutineAddresses(
                basicBlocks,
                structuredFlow,
                loopStructures);

        for (const u16 routineAddress : routineAddresses)
        {
            const auto* ifRoutine =
                structuredFlow.FindRoutine(
                    routineAddress);

            const auto* loopRoutine =
                loopStructures.FindRoutine(
                    routineAddress);

            const auto* blockRoutine =
                basicBlocks.FindRoutine(
                    routineAddress);

            std::vector<StructureRecord> records =
                BuildRecords(
                    ifRoutine,
                    loopRoutine);

            LowerWideMemoryChanges(
                project,
                blockRoutine,
                records,
                routineAddress,
                semanticConditions);

            AssignParents(records);
            BreakParentCycles(records);

            const std::vector<StatementRecord> statements =
                BuildStatements(
                    project,
                    metadata,
                    blockRoutine,
                    records,
                    routineAddress,
                    semanticConditions);

            StructuredExpression root;

            root.kind =
                StructuredExpressionKind::Block;

            root.address =
                routineAddress;

            if (ifRoutine != nullptr &&
                !ifRoutine->routineName.empty())
            {
                root.statement =
                    ifRoutine->routineName;
            }
            else if (loopRoutine != nullptr)
            {
                root.statement =
                    loopRoutine->routineName;
            }
            else if (blockRoutine != nullptr)
            {
                root.statement =
                    blockRoutine->routineName;
            }

            root.children =
                BuildChildren(
                    records,
                    statements,
                    std::nullopt,
                    ParentArm::Root,
                    routineAddress,
                    semanticConditions);

            if (!root.children.empty())
            {
                result.roots.push_back(
                    std::move(root));
            }
        }

        return result;
    }

private:

    enum class StructureKind
    {
        If,
        Loop
    };

    enum class ParentArm
    {
        Root,
        Then,
        Else,
        LoopBody
    };

    struct StructureRecord
    {
        StructureKind kind =
            StructureKind::If;

        u16 address = 0;

        const StructuredIf* ifStatement =
            nullptr;

        const StructuredLoop* loop =
            nullptr;

        std::vector<u16> footprint;

        std::optional<std::size_t> parentIndex;

        ParentArm parentArm =
            ParentArm::Root;

        bool loweredToStatement = false;

        u16 loweredInstructionAddress = 0;
        u16 loweredBodyInstructionAddress = 0;

        std::vector<u16>
            loweredAdditionalInstructionAddresses;

        std::string loweredStatement;
    };

    struct StatementRecord
    {
        StructuredExpression expression;

        std::optional<std::size_t> parentIndex;

        ParentArm parentArm =
            ParentArm::Root;
    };

    enum class TrackedValue
    {
        Accumulator,
        X,
        Y,
        NegativeZero,
        Carry,
        Overflow
    };

    enum class KnownBitState
    {
        Unreachable,
        Unknown,
        Clear,
        Set
    };

    enum class AccumulatorSourceKind
    {
        Unreachable,
        Unknown,
        X,
        Y,
        Immediate
    };

    struct AccumulatorSourceState
    {
        AccumulatorSourceKind kind =
            AccumulatorSourceKind::Unknown;

        u8 immediate = 0;

        bool operator==(
            const AccumulatorSourceState&) const = default;
    };

    enum class TrackedBit
    {
        Carry,
        Decimal
    };

    struct TrackedValueBlockFacts
    {
        bool useBeforeDefinition = false;
        bool defines = false;
    };

    struct ParentCandidate
    {
        std::size_t index = 0;
        std::size_t containerSize = 0;

        ParentArm arm =
            ParentArm::Root;

        bool explicitRelation = false;
    };

    [[nodiscard]]
    static std::vector<u16> CollectRoutineAddresses(
        const BasicBlockAnalysisResult& basicBlocks,
        const StructuredControlFlowAnalysisResult& structuredFlow,
        const LoopStructureAnalysisResult& loopStructures)
    {
        std::vector<u16> addresses;

        addresses.reserve(
            basicBlocks.routines.size() +
            structuredFlow.routines.size() +
            loopStructures.routines.size());

        for (const auto& routine : basicBlocks.routines)
        {
            addresses.push_back(
                routine.routineEntryAddress);
        }

        for (const auto& routine : structuredFlow.routines)
        {
            addresses.push_back(
                routine.routineEntryAddress);
        }

        for (const auto& routine : loopStructures.routines)
        {
            addresses.push_back(
                routine.routineEntryAddress);
        }

        SortUnique(addresses);

        return addresses;
    }

    [[nodiscard]]
    static std::vector<StructureRecord> BuildRecords(
        const RoutineStructuredControlFlowAnalysis* ifRoutine,
        const RoutineLoopStructureAnalysis* loopRoutine)
    {
        std::vector<StructureRecord> records;

        const std::size_t ifCount =
            ifRoutine != nullptr
                ? ifRoutine->ifStatements.size()
                : 0;

        const std::size_t loopCount =
            loopRoutine != nullptr
                ? loopRoutine->loops.size()
                : 0;

        records.reserve(
            ifCount + loopCount);

        if (ifRoutine != nullptr)
        {
            for (const auto& statement :
                 ifRoutine->ifStatements)
            {
                StructureRecord record;

                record.kind =
                    StructureKind::If;

                record.address =
                    statement.headerAddress;

                record.ifStatement =
                    &statement;

                record.footprint =
                    BuildIfFootprint(
                        statement);

                records.push_back(
                    std::move(record));
            }
        }

        if (loopRoutine != nullptr)
        {
            for (const auto& loop :
                 loopRoutine->loops)
            {
                StructureRecord record;

                record.kind =
                    StructureKind::Loop;

                record.address =
                    loop.headerAddress;

                record.loop =
                    &loop;

                record.footprint =
                    loop.blockAddresses;

                record.footprint.push_back(
                    loop.headerAddress);

                SortUnique(
                    record.footprint);

                records.push_back(
                    std::move(record));
            }
        }

        std::sort(
            records.begin(),
            records.end(),
            [](const StructureRecord& left,
               const StructureRecord& right)
            {
                if (left.address != right.address)
                {
                    return
                        left.address < right.address;
                }

                return
                    static_cast<int>(left.kind) <
                    static_cast<int>(right.kind);
            });

        return records;
    }

    static void LowerWideMemoryChanges(
        const Project& project,
        const RoutineBasicBlocks* routine,
        std::vector<StructureRecord>& records,
        u16 routineAddress,
        const SemanticConditionAnalysisResult& semanticConditions)
    {
        if (routine == nullptr)
        {
            return;
        }

        Disassembler disassembler;

        StructuredStatementFormatter formatter;

        for (auto& record : records)
        {
            const StructuredIf* statement =
                record.ifStatement;

            if (statement == nullptr ||
                statement->sourceInstruction !=
                    cpu6502::Instruction::BNE ||
                statement->flag !=
                    ProcessorFlag::Zero ||
                statement->thenState !=
                    FlagState::Set ||
                !statement->elseBlocks.empty() ||
                statement->thenBlocks.size() != 1)
            {
                continue;
            }

            const auto* semantic =
                semanticConditions.Find(
                    routineAddress,
                    statement->instructionAddress);

            if (semantic == nullptr ||
                !semantic->producerFound ||
                !semantic->semanticResolved)
            {
                continue;
            }

            const BasicBlock* headerBlock =
                routine->FindBlock(
                    statement->headerAddress);

            const BasicBlock* bodyBlock =
                routine->FindBlock(
                    statement->thenBlocks.front());

            if (headerBlock == nullptr ||
                bodyBlock == nullptr ||
                bodyBlock->instructionAddresses.size() != 1 ||
                !ContainsAddress(
                    headerBlock->instructionAddresses,
                    semantic->producerAddress) ||
                !ContainsAddress(
                    headerBlock->instructionAddresses,
                    statement->instructionAddress))
            {
                continue;
            }

            const u16 highInstructionAddress =
                bodyBlock->instructionAddresses.front();

            const auto low =
                disassembler.Decode(
                    project.GetMemory(),
                    semantic->producerAddress);

            const auto high =
                disassembler.Decode(
                    project.GetMemory(),
                    highInstructionAddress);

            std::string text;

            u16 additionalInstructionAddress = 0;

            if (semantic->producerInstruction ==
                cpu6502::Instruction::INC)
            {
                text =
                    formatter.FormatWideIncrement(
                        low,
                        high);
            }
            else if (semantic->producerInstruction ==
                     cpu6502::Instruction::LDA)
            {
                const BasicBlock* joinBlock =
                    routine->FindBlock(
                        statement->joinAddress);

                if (joinBlock == nullptr ||
                    joinBlock->
                        instructionAddresses.empty())
                {
                    continue;
                }

                additionalInstructionAddress =
                    joinBlock->
                        instructionAddresses.front();

                const auto lowDecrement =
                    disassembler.Decode(
                        project.GetMemory(),
                        additionalInstructionAddress);

                text =
                    formatter.FormatWideDecrement(
                        low,
                        high,
                        lowDecrement);
            }

            if (text.empty())
            {
                continue;
            }

            record.loweredToStatement =
                true;

            record.loweredInstructionAddress =
                semantic->producerAddress;

            record.loweredBodyInstructionAddress =
                highInstructionAddress;

            if (additionalInstructionAddress != 0)
            {
                record
                    .loweredAdditionalInstructionAddresses
                    .push_back(
                        additionalInstructionAddress);
            }

            record.loweredStatement =
                text;
        }
    }

    [[nodiscard]]
    static std::vector<u16> BuildIfFootprint(
        const StructuredIf& statement)
    {
        std::vector<u16> footprint;

        footprint.reserve(
            statement.thenBlocks.size() +
            statement.elseBlocks.size() +
            1);

        footprint.push_back(
            statement.headerAddress);

        footprint.insert(
            footprint.end(),
            statement.thenBlocks.begin(),
            statement.thenBlocks.end());

        footprint.insert(
            footprint.end(),
            statement.elseBlocks.begin(),
            statement.elseBlocks.end());

        SortUnique(
            footprint);

        return footprint;
    }

    static void AssignParents(
        std::vector<StructureRecord>& records)
    {
        for (std::size_t childIndex = 0;
             childIndex < records.size();
             ++childIndex)
        {
            std::optional<ParentCandidate> best;

            AddExplicitParentCandidate(
                records,
                childIndex,
                best);

            AddCrossKindParentCandidates(
                records,
                childIndex,
                best);

            if (!best.has_value())
            {
                continue;
            }

            records[childIndex].parentIndex =
                best->index;

            records[childIndex].parentArm =
                best->arm;
        }
    }

    static void AddExplicitParentCandidate(
        const std::vector<StructureRecord>& records,
        std::size_t childIndex,
        std::optional<ParentCandidate>& best)
    {
        const auto& child =
            records[childIndex];

        if (child.kind == StructureKind::If)
        {
            if (child.ifStatement == nullptr ||
                !child.ifStatement->
                    parentHeaderAddress.has_value())
            {
                return;
            }

            const auto parentIndex =
                FindRecord(
                    records,
                    StructureKind::If,
                    child.ifStatement->
                        parentHeaderAddress.value());

            if (!parentIndex.has_value())
            {
                return;
            }

            const auto& parent =
                records[parentIndex.value()];

            ParentArm arm =
                ParentArm::Then;

            std::size_t containerSize =
                parent.footprint.size();

            if (parent.ifStatement != nullptr)
            {
                if (child.ifStatement->parentArm ==
                    StructuredArm::Else)
                {
                    arm =
                        ParentArm::Else;

                    containerSize =
                        UniqueCount(
                            parent.ifStatement->
                                elseBlocks);
                }
                else
                {
                    containerSize =
                        UniqueCount(
                            parent.ifStatement->
                                thenBlocks);
                }
            }

            ConsiderParent(
                ParentCandidate{
                    parentIndex.value(),
                    containerSize,
                    arm,
                    true},
                best);

            return;
        }

        if (child.loop == nullptr ||
            !child.loop->
                parentHeaderAddress.has_value())
        {
            return;
        }

        const auto parentIndex =
            FindRecord(
                records,
                StructureKind::Loop,
                child.loop->
                    parentHeaderAddress.value());

        if (!parentIndex.has_value())
        {
            return;
        }

        ConsiderParent(
            ParentCandidate{
                parentIndex.value(),
                records[parentIndex.value()].
                    footprint.size(),
                ParentArm::LoopBody,
                true},
            best);
    }

    static void AddCrossKindParentCandidates(
        const std::vector<StructureRecord>& records,
        std::size_t childIndex,
        std::optional<ParentCandidate>& best)
    {
        const auto& child =
            records[childIndex];

        for (std::size_t parentIndex = 0;
             parentIndex < records.size();
             ++parentIndex)
        {
            if (parentIndex == childIndex)
            {
                continue;
            }

            const auto& parent =
                records[parentIndex];

            if (parent.kind == child.kind)
            {
                continue;
            }

            if (child.kind == StructureKind::If)
            {
                if (parent.loop == nullptr ||
                    !ContainsAll(
                        parent.loop->
                            blockAddresses,
                        child.footprint))
                {
                    continue;
                }

                const std::size_t containerSize =
                    UniqueCount(
                        parent.loop->
                            blockAddresses);

                if (containerSize <=
                    child.footprint.size())
                {
                    continue;
                }

                ConsiderParent(
                    ParentCandidate{
                        parentIndex,
                        containerSize,
                        ParentArm::LoopBody,
                        false},
                    best);

                continue;
            }

            if (parent.ifStatement == nullptr)
            {
                continue;
            }

            AddIfArmCandidate(
                parent.ifStatement->thenBlocks,
                ParentArm::Then,
                parentIndex,
                child,
                best);

            AddIfArmCandidate(
                parent.ifStatement->elseBlocks,
                ParentArm::Else,
                parentIndex,
                child,
                best);
        }
    }

    static void AddIfArmCandidate(
        const std::vector<u16>& armBlocks,
        ParentArm arm,
        std::size_t parentIndex,
        const StructureRecord& child,
        std::optional<ParentCandidate>& best)
    {
        if (!ContainsAll(
                armBlocks,
                child.footprint))
        {
            return;
        }

        const std::size_t containerSize =
            UniqueCount(
                armBlocks);

        if (containerSize <=
            child.footprint.size())
        {
            return;
        }

        ConsiderParent(
            ParentCandidate{
                parentIndex,
                containerSize,
                arm,
                false},
            best);
    }

    static void ConsiderParent(
        const ParentCandidate& candidate,
        std::optional<ParentCandidate>& best)
    {
        if (!best.has_value() ||
            candidate.containerSize <
                best->containerSize ||
            (candidate.containerSize ==
                 best->containerSize &&
             candidate.explicitRelation &&
             !best->explicitRelation) ||
            (candidate.containerSize ==
                 best->containerSize &&
             candidate.explicitRelation ==
                 best->explicitRelation &&
             candidate.index < best->index))
        {
            best =
                candidate;
        }
    }

    static void BreakParentCycles(
        std::vector<StructureRecord>& records)
    {
        for (std::size_t start = 0;
             start < records.size();
             ++start)
        {
            std::vector<bool> visited(
                records.size(),
                false);

            std::size_t current =
                start;

            while (true)
            {
                const std::optional<std::size_t> parent =
                    records[current].parentIndex;

                if (!parent.has_value())
                {
                    break;
                }

                if (visited[current])
                {
                    records[start].
                        parentIndex.reset();

                    records[start].parentArm =
                        ParentArm::Root;

                    break;
                }

                visited[current] =
                    true;

                current =
                    parent.value();

                if (current >= records.size())
                {
                    records[start].
                        parentIndex.reset();

                    records[start].parentArm =
                        ParentArm::Root;

                    break;
                }
            }
        }
    }

    [[nodiscard]]
    static std::vector<StatementRecord> BuildStatements(
        const Project& project,
        const DisassemblyMetadata& metadata,
        const RoutineBasicBlocks* routine,
        const std::vector<StructureRecord>& records,
        u16 routineAddress,
        const SemanticConditionAnalysisResult& semanticConditions)
    {
        std::vector<StatementRecord> statements;

        if (routine == nullptr)
        {
            return statements;
        }

        std::vector<u16> suppressedAddresses =
            BuildSuppressedAddresses(
                records,
                routineAddress,
                semanticConditions);

        for (const auto& record : records)
        {
            if (!record.loweredToStatement)
            {
                continue;
            }

            suppressedAddresses.push_back(
                record.loweredInstructionAddress);

            suppressedAddresses.push_back(
                record.loweredBodyInstructionAddress);

            suppressedAddresses.insert(
                suppressedAddresses.end(),
                record
                    .loweredAdditionalInstructionAddresses
                    .begin(),
                record
                    .loweredAdditionalInstructionAddresses
                    .end());
        }

        SortUnique(
            suppressedAddresses);

        Disassembler disassembler;

        const std::vector<bool> accumulatorLiveOut =
            BuildTrackedValueLiveOut(
                project,
                *routine,
                disassembler,
                TrackedValue::Accumulator);

        const std::vector<bool> xLiveOut =
            BuildTrackedValueLiveOut(
                project,
                *routine,
                disassembler,
                TrackedValue::X);

        const std::vector<bool> yLiveOut =
            BuildTrackedValueLiveOut(
                project,
                *routine,
                disassembler,
                TrackedValue::Y);

        const std::vector<bool> negativeZeroLiveOut =
            BuildTrackedValueLiveOut(
                project,
                *routine,
                disassembler,
                TrackedValue::NegativeZero);

        const std::vector<bool> carryLiveOut =
            BuildTrackedValueLiveOut(
                project,
                *routine,
                disassembler,
                TrackedValue::Carry);

        const std::vector<bool> overflowLiveOut =
            BuildTrackedValueLiveOut(
                project,
                *routine,
                disassembler,
                TrackedValue::Overflow);

        const std::vector<KnownBitState> decimalStateIn =
            BuildKnownBitStateIn(
                project,
                *routine,
                disassembler,
                TrackedBit::Decimal);

        const std::vector<KnownBitState> carryStateIn =
            BuildKnownBitStateIn(
                project,
                *routine,
                disassembler,
                TrackedBit::Carry);

        const std::vector<AccumulatorSourceState>
            accumulatorSourceStateIn =
                BuildAccumulatorSourceStateIn(
                    project,
                    *routine,
                    disassembler);

        StructuredStatementFormatter formatter;

        for (const auto& record : records)
        {
            if (!record.loweredToStatement)
            {
                continue;
            }

            AddStatement(
                statements,
                records,
                record.address,
                record.loweredInstructionAddress,
                record.loweredStatement);
        }

        for (std::size_t blockIndex = 0;
             blockIndex < routine->blocks.size();
             ++blockIndex)
        {
            const auto& block =
                routine->blocks[blockIndex];

            std::vector<DisassembledInstruction>
                instructions;

            instructions.reserve(
                block.instructionAddresses.size());

            for (const u16 instructionAddress :
                 block.instructionAddresses)
            {
                instructions.push_back(
                    disassembler.Decode(
                        project.GetMemory(),
                        instructionAddress));
            }

            const std::vector<bool> accumulatorLiveAfter =
                BuildTrackedValueLiveAfter(
                    instructions,
                    accumulatorLiveOut[blockIndex],
                    TrackedValue::Accumulator);

            const std::vector<bool> xLiveAfter =
                BuildTrackedValueLiveAfter(
                    instructions,
                    xLiveOut[blockIndex],
                    TrackedValue::X);

            const std::vector<bool> yLiveAfter =
                BuildTrackedValueLiveAfter(
                    instructions,
                    yLiveOut[blockIndex],
                    TrackedValue::Y);

            const std::vector<bool> negativeZeroLiveAfter =
                BuildTrackedValueLiveAfter(
                    instructions,
                    negativeZeroLiveOut[blockIndex],
                    TrackedValue::NegativeZero);

            const std::vector<bool> carryLiveAfter =
                BuildTrackedValueLiveAfter(
                    instructions,
                    carryLiveOut[blockIndex],
                    TrackedValue::Carry);

            const std::vector<bool> overflowLiveAfter =
                BuildTrackedValueLiveAfter(
                    instructions,
                    overflowLiveOut[blockIndex],
                    TrackedValue::Overflow);

            const std::vector<KnownBitState> decimalStateBefore =
                BuildKnownBitStateBefore(
                    instructions,
                    decimalStateIn[blockIndex],
                    TrackedBit::Decimal);

            const std::vector<KnownBitState> carryStateBefore =
                BuildKnownBitStateBefore(
                    instructions,
                    carryStateIn[blockIndex],
                    TrackedBit::Carry);

            const std::vector<AccumulatorSourceState>
                accumulatorSourceStateBefore =
                    BuildAccumulatorSourceStateBefore(
                        instructions,
                        accumulatorSourceStateIn[
                            blockIndex]);

            const std::vector<u16> foldedCarrySetups =
                BuildFoldedCarrySetups(
                    instructions,
                    suppressedAddresses);

            for (std::size_t instructionIndex = 0;
                 instructionIndex < instructions.size();)
            {
                const auto& instruction =
                    instructions[instructionIndex];

                if (std::binary_search(
                        foldedCarrySetups.begin(),
                        foldedCarrySetups.end(),
                        instruction.address))
                {
                    ++instructionIndex;
                    continue;
                }

                const std::size_t repeatedMemoryChangeCount =
                    CountRepeatedMemoryChanges(
                        instructions,
                        suppressedAddresses,
                        instructionIndex);

                const std::size_t changeLoadIndex =
                    instructionIndex +
                    repeatedMemoryChangeCount;

                if (repeatedMemoryChangeCount != 0 &&
                    changeLoadIndex < instructions.size() &&
                    !IsSuppressed(
                        suppressedAddresses,
                        instructions[
                            changeLoadIndex].address))
                {
                    const std::string text =
                        formatter.FormatMemoryChangeLoad(
                            instruction,
                            repeatedMemoryChangeCount,
                            instructions[
                                changeLoadIndex]);

                    if (!text.empty())
                    {
                        AddStatement(
                            statements,
                            records,
                            block.beginAddress,
                            instruction.address,
                            text);

                        instructionIndex =
                            changeLoadIndex + 1;

                        continue;
                    }
                }

                if (repeatedMemoryChangeCount >= 2)
                {
                    const std::size_t lastIndex =
                        instructionIndex +
                        repeatedMemoryChangeCount -
                        1;

                    AddStatement(
                        statements,
                        records,
                        block.beginAddress,
                        instruction.address,
                        formatter.FormatRepeatedMemoryChange(
                            instruction,
                            repeatedMemoryChangeCount,
                            negativeZeroLiveAfter[
                                lastIndex]));

                    instructionIndex +=
                        repeatedMemoryChangeCount;

                    continue;
                }

                if (instructionIndex + 2 <
                        instructions.size() &&
                    !accumulatorLiveAfter[
                        instructionIndex + 2] &&
                    !negativeZeroLiveAfter[
                        instructionIndex + 2] &&
                    !IsSuppressed(
                        suppressedAddresses,
                        instruction.address) &&
                    !IsSuppressed(
                        suppressedAddresses,
                        instructions[
                            instructionIndex + 1].
                            address) &&
                    !IsSuppressed(
                        suppressedAddresses,
                        instructions[
                            instructionIndex + 2].
                            address))
                {
                    const std::string text =
                        formatter.FormatBitwiseTransfer(
                            instruction,
                            instructions[
                                instructionIndex + 1],
                            instructions[
                                instructionIndex + 2]);

                    if (!text.empty())
                    {
                        AddStatement(
                            statements,
                            records,
                            block.beginAddress,
                            instruction.address,
                            text);

                        instructionIndex += 3;
                        continue;
                    }
                }

                if (instructionIndex + 1 <
                        instructions.size() &&
                    !accumulatorLiveAfter[
                        instructionIndex + 1] &&
                    !negativeZeroLiveAfter[
                        instructionIndex + 1] &&
                    !IsSuppressed(
                        suppressedAddresses,
                        instruction.address) &&
                    !IsSuppressed(
                        suppressedAddresses,
                        instructions[
                            instructionIndex + 1].
                            address))
                {
                    const std::string text =
                        formatter
                            .FormatAccumulatorBitwiseTransfer(
                                instruction,
                                instructions[
                                    instructionIndex + 1],
                                AccumulatorSourceText(
                                    accumulatorSourceStateBefore[
                                        instructionIndex]));

                    if (!text.empty())
                    {
                        AddStatement(
                            statements,
                            records,
                            block.beginAddress,
                            instruction.address,
                            text);

                        instructionIndex += 2;
                        continue;
                    }
                }

                if (!IsSuppressed(
                        suppressedAddresses,
                        instruction.address) &&
                    instructionIndex + 1 <
                        instructions.size())
                {
                    const std::string text =
                        formatter.FormatAccumulatorBitwise(
                            instruction,
                            instructions[
                                instructionIndex + 1]);

                    if (!text.empty())
                    {
                        AddStatement(
                            statements,
                            records,
                            block.beginAddress,
                            instruction.address,
                            text);

                        instructionIndex += 2;
                        continue;
                    }
                }

                if (CanCollapseWideArithmeticTransfer(
                        instructions,
                        suppressedAddresses,
                        instructionIndex))
                {
                    const std::string text =
                        formatter.FormatWideArithmeticTransfer(
                            instructions[
                                instructionIndex],
                            instructions[
                                instructionIndex + 1],
                            instructions[
                                instructionIndex + 2],
                            instructions[
                                instructionIndex + 3],
                            instructions[
                                instructionIndex + 4],
                            instructions[
                                instructionIndex + 5],
                            CarryInputText(
                                carryStateBefore[
                                    instructionIndex + 1]));

                    if (!text.empty())
                    {
                        AddStatement(
                            statements,
                            records,
                            block.beginAddress,
                            instruction.address,
                            text);

                        instructionIndex += 6;
                        continue;
                    }
                }

                if (CanCollapseArithmeticTransfer(
                        instructions,
                        accumulatorLiveAfter,
                        suppressedAddresses,
                        instructionIndex))
                {
                    const std::size_t operationIndex =
                        instructionIndex + 1;

                    const std::size_t storeIndex =
                        instructionIndex + 2;

                    AddStatement(
                        statements,
                        records,
                        block.beginAddress,
                        instruction.address,
                        formatter.FormatArithmeticTransfer(
                            instruction,
                            instructions[operationIndex],
                            instructions[storeIndex],
                            CarryInputText(
                                carryStateBefore[
                                    operationIndex]),
                            carryLiveAfter[storeIndex],
                            overflowLiveAfter[storeIndex],
                            negativeZeroLiveAfter[storeIndex],
                            decimalStateBefore[
                                operationIndex] ==
                                KnownBitState::Clear));

                    instructionIndex += 3;
                    continue;
                }

                if (CanCollapseRegisterTransfer(
                        instructions,
                        accumulatorLiveAfter,
                        xLiveAfter,
                        yLiveAfter,
                        negativeZeroLiveAfter,
                        suppressedAddresses,
                        instructionIndex))
                {
                    const std::string text =
                        formatter
                            .FormatRegisterTransfer(
                                instruction,
                                instructions[
                                    instructionIndex + 1]);

                    AddStatement(
                        statements,
                        records,
                        block.beginAddress,
                        instruction.address,
                        text);

                    instructionIndex += 2;
                    continue;
                }

                const std::size_t currentIndex =
                    instructionIndex;

                ++instructionIndex;

                if (IsSuppressed(
                        suppressedAddresses,
                        instruction.address))
                {
                    continue;
                }

                std::string text;

                if (IsCarryOperation(
                        instruction.instruction))
                {
                    text =
                        formatter.FormatCarryOperation(
                            instruction,
                            CarryInputText(
                                carryStateBefore[
                                    currentIndex]),
                            carryLiveAfter[
                                currentIndex],
                            overflowLiveAfter[
                                currentIndex],
                            negativeZeroLiveAfter[
                                currentIndex],
                            decimalStateBefore[
                                currentIndex] ==
                                KnownBitState::Clear,
                            AccumulatorSourceText(
                                accumulatorSourceStateBefore[
                                    currentIndex]));
                }
                else if (instruction.instruction ==
                         cpu6502::Instruction::STA)
                {
                    text =
                        formatter.FormatAccumulatorStore(
                            instruction,
                            AccumulatorSourceText(
                                accumulatorSourceStateBefore[
                                    currentIndex]));
                }
                else
                {
                    const AccumulatorSourceState
                        accumulatorSource =
                            accumulatorSourceStateBefore[
                                currentIndex];

                    const bool redundantTransfer =
                        !negativeZeroLiveAfter[
                            currentIndex] &&
                        ((instruction.instruction ==
                              cpu6502::Instruction::TAX &&
                          accumulatorSource.kind ==
                              AccumulatorSourceKind::X) ||
                         (instruction.instruction ==
                              cpu6502::Instruction::TAY &&
                          accumulatorSource.kind ==
                              AccumulatorSourceKind::Y));

                    if (redundantTransfer)
                    {
                        continue;
                    }

                    text =
                        formatter.FormatStackPull(
                            instruction,
                            negativeZeroLiveAfter[
                                currentIndex]);

                    if (text.empty())
                    {
                        text =
                            formatter.FormatIncrementDecrement(
                                instruction,
                                negativeZeroLiveAfter[
                                    currentIndex]);
                    }

                    if (text.empty())
                    {
                        text =
                            formatter.FormatLoad(
                                instruction,
                                negativeZeroLiveAfter[
                                    currentIndex]);
                    }

                    if (text.empty())
                    {
                        text =
                            formatter
                                .FormatSourceRegisterTransfer(
                                    instruction,
                                    negativeZeroLiveAfter[
                                        currentIndex]);
                    }

                    if (text.empty())
                    {
                        text =
                            formatter
                                .FormatAccumulatorRegisterTransfer(
                                    instruction,
                                    AccumulatorSourceText(
                                        accumulatorSource),
                                    negativeZeroLiveAfter[
                                        currentIndex]);
                    }

                    if (text.empty() &&
                        HasKnownAccumulatorSource(
                            accumulatorSource))
                    {
                        text =
                            formatter
                                .FormatAccumulatorBitwiseOperation(
                                    instruction,
                                    AccumulatorSourceText(
                                        accumulatorSource));
                    }

                    if (text.empty())
                    {
                        text =
                            formatter.FormatAccumulatorConsumer(
                                instruction,
                                AccumulatorSourceText(
                                    accumulatorSource),
                                carryLiveAfter[
                                    currentIndex],
                                overflowLiveAfter[
                                    currentIndex],
                                negativeZeroLiveAfter[
                                    currentIndex]);
                    }

                    if (text.empty())
                    {
                        text =
                            FormatStatement(
                                metadata,
                                instruction);
                    }
                }

                AddStatement(
                    statements,
                    records,
                    block.beginAddress,
                    instruction.address,
                    text);
            }
        }

        return statements;
    }

    static void AddStatement(
        std::vector<StatementRecord>& statements,
        const std::vector<StructureRecord>& records,
        u16 blockAddress,
        u16 instructionAddress,
        std::string text)
    {
        if (text.empty())
        {
            return;
        }

        StatementRecord statement;

        statement.expression.kind =
            StructuredExpressionKind::Statement;

        statement.expression.address =
            instructionAddress;

        statement.expression.statement =
            std::move(text);

        AssignStatementParent(
            statement,
            records,
            blockAddress);

        statements.push_back(
            std::move(statement));
    }

    [[nodiscard]]
    static std::size_t CountRepeatedMemoryChanges(
        const std::vector<DisassembledInstruction>& instructions,
        const std::vector<u16>& suppressedAddresses,
        std::size_t instructionIndex)
    {
        if (instructionIndex >=
            instructions.size())
        {
            return 0;
        }

        const auto& first =
            instructions[instructionIndex];

        if (first.instruction !=
                cpu6502::Instruction::INC &&
            first.instruction !=
                cpu6502::Instruction::DEC)
        {
            return 0;
        }

        std::size_t count = 0;

        for (std::size_t index = instructionIndex;
             index < instructions.size();
             ++index)
        {
            const auto& candidate =
                instructions[index];

            if (!SameMemoryOperation(
                    first,
                    candidate) ||
                IsSuppressed(
                    suppressedAddresses,
                    candidate.address))
            {
                break;
            }

            ++count;
        }

        return count;
    }

    [[nodiscard]]
    static bool SameMemoryOperation(
        const DisassembledInstruction& left,
        const DisassembledInstruction& right)
    {
        return
            left.instruction ==
                right.instruction &&
            left.addressMode ==
                right.addressMode &&
            left.length ==
                right.length &&
            left.bytes[1] ==
                right.bytes[1] &&
            left.bytes[2] ==
                right.bytes[2];
    }

    [[nodiscard]]
    static bool CanCollapseWideArithmeticTransfer(
        const std::vector<DisassembledInstruction>& instructions,
        const std::vector<u16>& suppressedAddresses,
        std::size_t instructionIndex)
    {
        if (instructionIndex + 5 >=
            instructions.size())
        {
            return false;
        }

        const auto& lowLoad =
            instructions[instructionIndex];

        const auto& lowOperation =
            instructions[instructionIndex + 1];

        const auto& lowStore =
            instructions[instructionIndex + 2];

        const auto& highLoad =
            instructions[instructionIndex + 3];

        const auto& highOperation =
            instructions[instructionIndex + 4];

        const auto& highStore =
            instructions[instructionIndex + 5];

        const bool addition =
            lowOperation.instruction ==
                cpu6502::Instruction::ADC &&
            highOperation.instruction ==
                cpu6502::Instruction::ADC;

        const bool subtraction =
            lowOperation.instruction ==
                cpu6502::Instruction::SBC &&
            highOperation.instruction ==
                cpu6502::Instruction::SBC;

        if (lowLoad.instruction !=
                cpu6502::Instruction::LDA ||
            highLoad.instruction !=
                cpu6502::Instruction::LDA ||
            lowStore.instruction !=
                cpu6502::Instruction::STA ||
            highStore.instruction !=
                cpu6502::Instruction::STA ||
            (!addition && !subtraction))
        {
            return false;
        }

        for (std::size_t offset = 0;
             offset < 6;
             ++offset)
        {
            if (IsSuppressed(
                    suppressedAddresses,
                    instructions[
                        instructionIndex + offset].
                        address))
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]]
    static bool CanCollapseArithmeticTransfer(
        const std::vector<DisassembledInstruction>& instructions,
        const std::vector<bool>& accumulatorLiveAfter,
        const std::vector<u16>& suppressedAddresses,
        std::size_t instructionIndex)
    {
        if (instructionIndex + 2 >=
                instructions.size() ||
            instructionIndex + 2 >=
                accumulatorLiveAfter.size())
        {
            return false;
        }

        const auto& load =
            instructions[instructionIndex];

        const auto& operation =
            instructions[instructionIndex + 1];

        const auto& store =
            instructions[instructionIndex + 2];

        const bool arithmetic =
            operation.instruction ==
                cpu6502::Instruction::ADC ||
            operation.instruction ==
                cpu6502::Instruction::SBC;

        return
            load.instruction ==
                cpu6502::Instruction::LDA &&
            arithmetic &&
            store.instruction ==
                cpu6502::Instruction::STA &&
            !accumulatorLiveAfter[
                instructionIndex + 2] &&
            !IsSuppressed(
                suppressedAddresses,
                load.address) &&
            !IsSuppressed(
                suppressedAddresses,
                operation.address) &&
            !IsSuppressed(
                suppressedAddresses,
                store.address);
    }

    [[nodiscard]]
    static bool CanCollapseRegisterTransfer(
        const std::vector<DisassembledInstruction>& instructions,
        const std::vector<bool>& accumulatorLiveAfter,
        const std::vector<bool>& xLiveAfter,
        const std::vector<bool>& yLiveAfter,
        const std::vector<bool>& negativeZeroLiveAfter,
        const std::vector<u16>& suppressedAddresses,
        std::size_t instructionIndex)
    {
        if (instructionIndex + 1 >=
                instructions.size() ||
            instructionIndex + 1 >=
                accumulatorLiveAfter.size() ||
            instructionIndex + 1 >=
                xLiveAfter.size() ||
            instructionIndex + 1 >=
                yLiveAfter.size() ||
            instructionIndex + 1 >=
                negativeZeroLiveAfter.size())
        {
            return false;
        }

        const auto& load =
            instructions[instructionIndex];

        const auto& store =
            instructions[instructionIndex + 1];

        const auto trackedValue =
            RegisterTransferValue(
                load,
                store);

        if (!trackedValue.has_value() ||
            negativeZeroLiveAfter[
                instructionIndex + 1])
        {
            return false;
        }

        bool registerLive = true;

        switch (trackedValue.value())
        {
        case TrackedValue::Accumulator:
            registerLive =
                accumulatorLiveAfter[
                    instructionIndex + 1];
            break;

        case TrackedValue::X:
            registerLive =
                xLiveAfter[
                    instructionIndex + 1];
            break;

        case TrackedValue::Y:
            registerLive =
                yLiveAfter[
                    instructionIndex + 1];
            break;

        case TrackedValue::NegativeZero:
        case TrackedValue::Carry:
        case TrackedValue::Overflow:
            return false;
        }

        return
            !registerLive &&
            !IsSuppressed(
                suppressedAddresses,
                load.address) &&
            !IsSuppressed(
                suppressedAddresses,
                store.address);
    }

    [[nodiscard]]
    static std::optional<TrackedValue>
    RegisterTransferValue(
        const DisassembledInstruction& load,
        const DisassembledInstruction& store)
    {
        using Instruction =
            cpu6502::Instruction;

        if (load.instruction == Instruction::LDA &&
            store.instruction == Instruction::STA)
        {
            return TrackedValue::Accumulator;
        }

        if (load.instruction == Instruction::LDX &&
            store.instruction == Instruction::STX)
        {
            return TrackedValue::X;
        }

        if (load.instruction == Instruction::LDY &&
            store.instruction == Instruction::STY)
        {
            return TrackedValue::Y;
        }

        return std::nullopt;
    }

    [[nodiscard]]
    static std::vector<u16> BuildFoldedCarrySetups(
        const std::vector<DisassembledInstruction>& instructions,
        const std::vector<u16>& suppressedAddresses)
    {
        std::vector<u16> addresses;

        for (std::size_t setupIndex = 0;
             setupIndex < instructions.size();
             ++setupIndex)
        {
            const auto& setup =
                instructions[setupIndex];

            const bool explicitCarry =
                setup.instruction ==
                    cpu6502::Instruction::CLC ||
                setup.instruction ==
                    cpu6502::Instruction::SEC;

            if (!explicitCarry ||
                IsSuppressed(
                    suppressedAddresses,
                    setup.address))
            {
                continue;
            }

            for (std::size_t useIndex =
                     setupIndex + 1;
                 useIndex < instructions.size();
                 ++useIndex)
            {
                const auto& candidate =
                    instructions[useIndex];

                if (ReadsTrackedValue(
                        candidate,
                        TrackedValue::Carry))
                {
                    if (IsCarryOperation(
                            candidate.instruction) &&
                        !IsSuppressed(
                            suppressedAddresses,
                            candidate.address))
                    {
                        addresses.push_back(
                            setup.address);
                    }

                    break;
                }

                if (WritesTrackedValue(
                        candidate,
                        TrackedValue::Carry))
                {
                    break;
                }
            }
        }

        SortUnique(addresses);

        return addresses;
    }

    [[nodiscard]]
    static std::string CarryInputText(
        KnownBitState state)
    {
        switch (state)
        {
        case KnownBitState::Clear:
            return "0";

        case KnownBitState::Set:
            return "1";

        case KnownBitState::Unreachable:
        case KnownBitState::Unknown:
        default:
            return "C";
        }
    }

    [[nodiscard]]
    static bool IsCarryOperation(
        cpu6502::Instruction instruction)
    {
        return
            instruction ==
                cpu6502::Instruction::ASL ||
            instruction ==
                cpu6502::Instruction::LSR ||
            instruction ==
                cpu6502::Instruction::ADC ||
            instruction ==
                cpu6502::Instruction::SBC ||
            instruction ==
                cpu6502::Instruction::ROL ||
            instruction ==
                cpu6502::Instruction::ROR;
    }

    [[nodiscard]]
    static std::vector<AccumulatorSourceState>
    BuildAccumulatorSourceStateIn(
        const Project& project,
        const RoutineBasicBlocks& routine,
        const Disassembler& disassembler)
    {
        const std::size_t blockCount =
            routine.blocks.size();

        std::vector<AccumulatorSourceState> stateIn(
            blockCount,
            {
                AccumulatorSourceKind::Unreachable,
                0});

        std::vector<AccumulatorSourceState> stateOut(
            blockCount,
            {
                AccumulatorSourceKind::Unreachable,
                0});

        if (blockCount == 0)
        {
            return stateIn;
        }

        const std::size_t entryIndex =
            FindBlockIndex(
                routine,
                routine.routineEntryAddress)
                .value_or(0);

        stateIn[entryIndex] =
            {
                AccumulatorSourceKind::Unknown,
                0};

        bool changed = true;

        while (changed)
        {
            changed = false;

            for (std::size_t index = 0;
                 index < blockCount;
                 ++index)
            {
                if (stateIn[index].kind ==
                    AccumulatorSourceKind::Unreachable)
                {
                    continue;
                }

                AccumulatorSourceState newStateOut =
                    stateIn[index];

                for (const u16 address :
                     routine.blocks[index].
                         instructionAddresses)
                {
                    newStateOut =
                        ApplyAccumulatorSourceInstruction(
                            newStateOut,
                            disassembler.Decode(
                                project.GetMemory(),
                                address));
                }

                if (stateOut[index] !=
                    newStateOut)
                {
                    stateOut[index] =
                        newStateOut;

                    changed = true;
                }

                for (const auto& successor :
                     routine.blocks[index].successors)
                {
                    const auto successorIndex =
                        FindBlockIndex(
                            routine,
                            successor.targetAddress);

                    if (!successorIndex.has_value())
                    {
                        continue;
                    }

                    const AccumulatorSourceState joined =
                        JoinAccumulatorSourceStates(
                            stateIn[
                                successorIndex.value()],
                            newStateOut);

                    if (stateIn[
                            successorIndex.value()] !=
                        joined)
                    {
                        stateIn[
                            successorIndex.value()] =
                            joined;

                        changed = true;
                    }
                }
            }
        }

        return stateIn;
    }

    [[nodiscard]]
    static std::vector<AccumulatorSourceState>
    BuildAccumulatorSourceStateBefore(
        const std::vector<DisassembledInstruction>& instructions,
        AccumulatorSourceState stateIn)
    {
        std::vector<AccumulatorSourceState> stateBefore(
            instructions.size(),
            {
                AccumulatorSourceKind::Unknown,
                0});

        AccumulatorSourceState state =
            stateIn.kind ==
                AccumulatorSourceKind::Unreachable
                ? AccumulatorSourceState{
                      AccumulatorSourceKind::Unknown,
                      0}
                : stateIn;

        for (std::size_t index = 0;
             index < instructions.size();
             ++index)
        {
            stateBefore[index] =
                state;

            state =
                ApplyAccumulatorSourceInstruction(
                    state,
                    instructions[index]);
        }

        return stateBefore;
    }

    [[nodiscard]]
    static AccumulatorSourceState
    ApplyAccumulatorSourceInstruction(
        AccumulatorSourceState state,
        const DisassembledInstruction& instruction)
    {
        using Instruction =
            cpu6502::Instruction;

        if (instruction.instruction ==
            Instruction::TXA)
        {
            return
                {
                    AccumulatorSourceKind::X,
                    0};
        }

        if (instruction.instruction ==
            Instruction::TYA)
        {
            return
                {
                    AccumulatorSourceKind::Y,
                    0};
        }

        if (instruction.instruction ==
                Instruction::LDA &&
            instruction.addressMode ==
                cpu6502::AddressMode::Immediate)
        {
            return
                {
                    AccumulatorSourceKind::Immediate,
                    instruction.bytes[1]};
        }

        if ((state.kind == AccumulatorSourceKind::X &&
             WritesTrackedValue(
                 instruction,
                 TrackedValue::X)) ||
            (state.kind == AccumulatorSourceKind::Y &&
             WritesTrackedValue(
                 instruction,
                 TrackedValue::Y)) ||
            WritesTrackedValue(
                instruction,
                TrackedValue::Accumulator))
        {
            return
                {
                    AccumulatorSourceKind::Unknown,
                    0};
        }

        return state;
    }

    [[nodiscard]]
    static AccumulatorSourceState
    JoinAccumulatorSourceStates(
        AccumulatorSourceState left,
        AccumulatorSourceState right)
    {
        if (left.kind ==
            AccumulatorSourceKind::Unreachable)
        {
            return right;
        }

        if (right.kind ==
            AccumulatorSourceKind::Unreachable)
        {
            return left;
        }

        if (left == right)
        {
            return left;
        }

        return
            {
                AccumulatorSourceKind::Unknown,
                0};
    }

    [[nodiscard]]
    static bool HasKnownAccumulatorSource(
        AccumulatorSourceState state)
    {
        return
            state.kind ==
                AccumulatorSourceKind::X ||
            state.kind ==
                AccumulatorSourceKind::Y ||
            state.kind ==
                AccumulatorSourceKind::Immediate;
    }

    [[nodiscard]]
    static std::string ByteHex(
        u8 value)
    {
        static constexpr char Digits[] =
            "0123456789ABCDEF";

        std::string result = "$00";

        result[1] =
            Digits[(value >> 4) & 0x0F];

        result[2] =
            Digits[value & 0x0F];

        return result;
    }

    [[nodiscard]]
    static std::string AccumulatorSourceText(
        AccumulatorSourceState state)
    {
        switch (state.kind)
        {
        case AccumulatorSourceKind::X:
            return "X";

        case AccumulatorSourceKind::Y:
            return "Y";

        case AccumulatorSourceKind::Immediate:
            return ByteHex(
                state.immediate);

        case AccumulatorSourceKind::Unreachable:
        case AccumulatorSourceKind::Unknown:
            return "A";
        }

        return "A";
    }

    [[nodiscard]]
    static std::vector<KnownBitState> BuildKnownBitStateIn(
        const Project& project,
        const RoutineBasicBlocks& routine,
        const Disassembler& disassembler,
        TrackedBit trackedBit)
    {
        const std::size_t blockCount =
            routine.blocks.size();

        std::vector<KnownBitState> stateIn(
            blockCount,
            KnownBitState::Unreachable);

        std::vector<KnownBitState> stateOut(
            blockCount,
            KnownBitState::Unreachable);

        if (blockCount == 0)
        {
            return stateIn;
        }

        const std::size_t entryIndex =
            FindBlockIndex(
                routine,
                routine.routineEntryAddress)
                .value_or(0);

        stateIn[entryIndex] =
            KnownBitState::Unknown;

        bool changed = true;

        while (changed)
        {
            changed = false;

            for (std::size_t index = 0;
                 index < blockCount;
                 ++index)
            {
                if (stateIn[index] ==
                    KnownBitState::Unreachable)
                {
                    continue;
                }

                KnownBitState newStateOut =
                    stateIn[index];

                for (const u16 address :
                     routine.blocks[index].
                         instructionAddresses)
                {
                    newStateOut =
                        ApplyKnownBitInstruction(
                            newStateOut,
                            disassembler.Decode(
                                project.GetMemory(),
                                address),
                            trackedBit);
                }

                if (stateOut[index] !=
                    newStateOut)
                {
                    stateOut[index] =
                        newStateOut;

                    changed = true;
                }

                for (const auto& successor :
                     routine.blocks[index].successors)
                {
                    const auto successorIndex =
                        FindBlockIndex(
                            routine,
                            successor.targetAddress);

                    if (!successorIndex.has_value())
                    {
                        continue;
                    }

                    const KnownBitState joined =
                        JoinKnownBitStates(
                            stateIn[
                                successorIndex.value()],
                            newStateOut);

                    if (stateIn[
                            successorIndex.value()] !=
                        joined)
                    {
                        stateIn[
                            successorIndex.value()] =
                            joined;

                        changed = true;
                    }
                }
            }
        }

        return stateIn;
    }

    [[nodiscard]]
    static std::vector<KnownBitState>
    BuildKnownBitStateBefore(
        const std::vector<DisassembledInstruction>& instructions,
        KnownBitState stateIn,
        TrackedBit trackedBit)
    {
        std::vector<KnownBitState> stateBefore(
            instructions.size(),
            KnownBitState::Unknown);

        KnownBitState state =
            stateIn == KnownBitState::Unreachable
                ? KnownBitState::Unknown
                : stateIn;

        for (std::size_t index = 0;
             index < instructions.size();
             ++index)
        {
            stateBefore[index] =
                state;

            state =
                ApplyKnownBitInstruction(
                    state,
                    instructions[index],
                    trackedBit);
        }

        return stateBefore;
    }

    [[nodiscard]]
    static KnownBitState ApplyKnownBitInstruction(
        KnownBitState state,
        const DisassembledInstruction& instruction,
        TrackedBit trackedBit)
    {
        using Instruction =
            cpu6502::Instruction;

        if (instruction.instruction ==
                Instruction::BRK ||
            instruction.instruction ==
                Instruction::Illegal ||
            instruction.instruction ==
                Instruction::JSR ||
            instruction.instruction ==
                Instruction::PLP ||
            instruction.instruction ==
                Instruction::RTI)
        {
            return KnownBitState::Unknown;
        }

        if (trackedBit == TrackedBit::Decimal)
        {
            if (instruction.instruction ==
                Instruction::CLD)
            {
                return KnownBitState::Clear;
            }

            if (instruction.instruction ==
                Instruction::SED)
            {
                return KnownBitState::Set;
            }

            return state;
        }

        if (instruction.instruction ==
            Instruction::CLC)
        {
            return KnownBitState::Clear;
        }

        if (instruction.instruction ==
            Instruction::SEC)
        {
            return KnownBitState::Set;
        }

        if (WritesCarry(
                instruction.instruction))
        {
            return KnownBitState::Unknown;
        }

        return state;
    }

    [[nodiscard]]
    static KnownBitState JoinKnownBitStates(
        KnownBitState left,
        KnownBitState right)
    {
        if (left == KnownBitState::Unreachable)
        {
            return right;
        }

        if (right == KnownBitState::Unreachable)
        {
            return left;
        }

        if (left == right)
        {
            return left;
        }

        return KnownBitState::Unknown;
    }

    [[nodiscard]]
    static bool IsSuppressed(
        const std::vector<u16>& suppressedAddresses,
        u16 address)
    {
        return
            std::binary_search(
                suppressedAddresses.begin(),
                suppressedAddresses.end(),
                address);
    }

    [[nodiscard]]
    static std::vector<bool> BuildTrackedValueLiveOut(
        const Project& project,
        const RoutineBasicBlocks& routine,
        const Disassembler& disassembler,
        TrackedValue trackedValue)
    {
        const std::size_t blockCount =
            routine.blocks.size();

        std::vector<TrackedValueBlockFacts> facts(
            blockCount);

        std::vector<bool> liveIn(
            blockCount,
            false);

        std::vector<bool> liveOut(
            blockCount,
            false);

        for (std::size_t index = 0;
             index < blockCount;
             ++index)
        {
            facts[index] =
                BuildTrackedValueBlockFacts(
                    project,
                    routine.blocks[index],
                    disassembler,
                    trackedValue);
        }

        bool changed = true;

        while (changed)
        {
            changed = false;

            for (std::size_t offset = 0;
                 offset < blockCount;
                 ++offset)
            {
                const std::size_t index =
                    blockCount - offset - 1;

                const auto& block =
                    routine.blocks[index];

                bool newLiveOut =
                    block.terminal ||
                    block.successors.empty();

                for (const auto& successor :
                     block.successors)
                {
                    const auto successorIndex =
                        FindBlockIndex(
                            routine,
                            successor.targetAddress);

                    if (!successorIndex.has_value())
                    {
                        newLiveOut = true;
                        continue;
                    }

                    newLiveOut =
                        newLiveOut ||
                        liveIn[
                            successorIndex.value()];
                }

                const bool newLiveIn =
                    facts[index].
                        useBeforeDefinition ||
                    (newLiveOut &&
                     !facts[index].defines);

                if (newLiveOut != liveOut[index] ||
                    newLiveIn != liveIn[index])
                {
                    liveOut[index] =
                        newLiveOut;

                    liveIn[index] =
                        newLiveIn;

                    changed = true;
                }
            }
        }

        return liveOut;
    }

    [[nodiscard]]
    static TrackedValueBlockFacts
    BuildTrackedValueBlockFacts(
        const Project& project,
        const BasicBlock& block,
        const Disassembler& disassembler,
        TrackedValue trackedValue)
    {
        TrackedValueBlockFacts facts;

        bool defined = false;

        for (const u16 address :
             block.instructionAddresses)
        {
            const auto instruction =
                disassembler.Decode(
                    project.GetMemory(),
                    address);

            if (ReadsTrackedValue(
                    instruction,
                    trackedValue) &&
                !defined)
            {
                facts.useBeforeDefinition =
                    true;
            }

            if (WritesTrackedValue(
                    instruction,
                    trackedValue))
            {
                facts.defines = true;
                defined = true;
            }
        }

        return facts;
    }

    [[nodiscard]]
    static std::vector<bool> BuildTrackedValueLiveAfter(
        const std::vector<DisassembledInstruction>& instructions,
        bool liveOut,
        TrackedValue trackedValue)
    {
        std::vector<bool> liveAfter(
            instructions.size(),
            false);

        bool live =
            liveOut;

        for (std::size_t offset = 0;
             offset < instructions.size();
             ++offset)
        {
            const std::size_t index =
                instructions.size() -
                offset -
                1;

            liveAfter[index] =
                live;

            live =
                ReadsTrackedValue(
                    instructions[index],
                    trackedValue) ||
                (live &&
                 !WritesTrackedValue(
                     instructions[index],
                     trackedValue));
        }

        return liveAfter;
    }

    [[nodiscard]]
    static std::optional<std::size_t> FindBlockIndex(
        const RoutineBasicBlocks& routine,
        u16 blockAddress)
    {
        for (std::size_t index = 0;
             index < routine.blocks.size();
             ++index)
        {
            if (routine.blocks[index].beginAddress ==
                blockAddress)
            {
                return index;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]]
    static bool ReadsTrackedValue(
        const DisassembledInstruction& instruction,
        TrackedValue trackedValue)
    {
        using Instruction =
            cpu6502::Instruction;

        if (instruction.instruction ==
                Instruction::JSR ||
            instruction.instruction ==
                Instruction::RTI ||
            instruction.instruction ==
                Instruction::RTS ||
            instruction.instruction ==
                Instruction::BRK ||
            instruction.instruction ==
                Instruction::Illegal)
        {
            return true;
        }

        switch (trackedValue)
        {
        case TrackedValue::Accumulator:
            switch (instruction.instruction)
            {
            case Instruction::ADC:
            case Instruction::AND:
            case Instruction::BIT:
            case Instruction::CMP:
            case Instruction::EOR:
            case Instruction::ORA:
            case Instruction::PHA:
            case Instruction::SBC:
            case Instruction::STA:
            case Instruction::TAX:
            case Instruction::TAY:
                return true;

            case Instruction::ASL:
            case Instruction::LSR:
            case Instruction::ROL:
            case Instruction::ROR:
                return
                    instruction.addressMode ==
                    cpu6502::AddressMode::Accumulator;

            default:
            return
                false;
            }

        case TrackedValue::X:
            return
                AddressModeReadsX(
                    instruction.addressMode) ||
                instruction.instruction ==
                    Instruction::CPX ||
                instruction.instruction ==
                    Instruction::DEX ||
                instruction.instruction ==
                    Instruction::INX ||
                instruction.instruction ==
                    Instruction::STX ||
                instruction.instruction ==
                    Instruction::TXA ||
                instruction.instruction ==
                    Instruction::TXS;

        case TrackedValue::Y:
            return
                AddressModeReadsY(
                    instruction.addressMode) ||
                instruction.instruction ==
                    Instruction::CPY ||
                instruction.instruction ==
                    Instruction::DEY ||
                instruction.instruction ==
                    Instruction::INY ||
                instruction.instruction ==
                    Instruction::STY ||
                instruction.instruction ==
                    Instruction::TYA;

        case TrackedValue::NegativeZero:
            return
                instruction.instruction ==
                    Instruction::BEQ ||
                instruction.instruction ==
                    Instruction::BNE ||
                instruction.instruction ==
                    Instruction::BMI ||
                instruction.instruction ==
                    Instruction::BPL ||
                instruction.instruction ==
                    Instruction::PHP;

        case TrackedValue::Carry:
            return
                instruction.instruction ==
                    Instruction::ADC ||
                instruction.instruction ==
                    Instruction::BCC ||
                instruction.instruction ==
                    Instruction::BCS ||
                instruction.instruction ==
                    Instruction::PHP ||
                instruction.instruction ==
                    Instruction::ROL ||
                instruction.instruction ==
                    Instruction::ROR ||
                instruction.instruction ==
                    Instruction::SBC;

        case TrackedValue::Overflow:
            return
                instruction.instruction ==
                    Instruction::BVC ||
                instruction.instruction ==
                    Instruction::BVS ||
                instruction.instruction ==
                    Instruction::PHP;
        }

        return false;
    }

    [[nodiscard]]
    static bool WritesTrackedValue(
        const DisassembledInstruction& instruction,
        TrackedValue trackedValue)
    {
        using Instruction =
            cpu6502::Instruction;

        if (instruction.instruction ==
                Instruction::JSR ||
            instruction.instruction ==
                Instruction::BRK ||
            instruction.instruction ==
                Instruction::Illegal)
        {
            return true;
        }

        switch (trackedValue)
        {
        case TrackedValue::Accumulator:
            switch (instruction.instruction)
            {
            case Instruction::ADC:
            case Instruction::AND:
            case Instruction::EOR:
            case Instruction::LDA:
            case Instruction::ORA:
            case Instruction::PLA:
            case Instruction::SBC:
            case Instruction::TXA:
            case Instruction::TYA:
                return true;

            case Instruction::ASL:
            case Instruction::LSR:
            case Instruction::ROL:
            case Instruction::ROR:
                return
                    instruction.addressMode ==
                    cpu6502::AddressMode::Accumulator;

            default:
            return
                false;
            }

        case TrackedValue::X:
            return
                instruction.instruction ==
                    Instruction::DEX ||
                instruction.instruction ==
                    Instruction::INX ||
                instruction.instruction ==
                    Instruction::LDX ||
                instruction.instruction ==
                    Instruction::TAX ||
                instruction.instruction ==
                    Instruction::TSX;

        case TrackedValue::Y:
            return
                instruction.instruction ==
                    Instruction::DEY ||
                instruction.instruction ==
                    Instruction::INY ||
                instruction.instruction ==
                    Instruction::LDY ||
                instruction.instruction ==
                    Instruction::TAY;

        case TrackedValue::NegativeZero:
            return
                WritesNegativeZero(
                    instruction.instruction);

        case TrackedValue::Carry:
            return
                WritesCarry(
                    instruction.instruction);

        case TrackedValue::Overflow:
            return
                WritesOverflow(
                    instruction.instruction);
        }

        return false;
    }

    [[nodiscard]]
    static bool AddressModeReadsX(
        cpu6502::AddressMode addressMode)
    {
        return
            addressMode ==
                cpu6502::AddressMode::ZeroPageX ||
            addressMode ==
                cpu6502::AddressMode::AbsoluteX ||
            addressMode ==
                cpu6502::AddressMode::IndexedIndirect;
    }

    [[nodiscard]]
    static bool AddressModeReadsY(
        cpu6502::AddressMode addressMode)
    {
        return
            addressMode ==
                cpu6502::AddressMode::ZeroPageY ||
            addressMode ==
                cpu6502::AddressMode::AbsoluteY ||
            addressMode ==
                cpu6502::AddressMode::IndirectIndexed;
    }

    [[nodiscard]]
    static bool WritesNegativeZero(
        cpu6502::Instruction instruction)
    {
        using Instruction =
            cpu6502::Instruction;

        switch (instruction)
        {
        case Instruction::ADC:
        case Instruction::AND:
        case Instruction::ASL:
        case Instruction::BIT:
        case Instruction::CMP:
        case Instruction::CPX:
        case Instruction::CPY:
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
        case Instruction::PLP:
        case Instruction::ROL:
        case Instruction::ROR:
        case Instruction::RTI:
        case Instruction::SBC:
        case Instruction::TAX:
        case Instruction::TAY:
        case Instruction::TSX:
        case Instruction::TXA:
        case Instruction::TYA:
            return true;

        default:
            return false;
        }
    }

    [[nodiscard]]
    static bool WritesCarry(
        cpu6502::Instruction instruction)
    {
        using Instruction =
            cpu6502::Instruction;

        switch (instruction)
        {
        case Instruction::ADC:
        case Instruction::ASL:
        case Instruction::CLC:
        case Instruction::CMP:
        case Instruction::CPX:
        case Instruction::CPY:
        case Instruction::LSR:
        case Instruction::PLP:
        case Instruction::ROL:
        case Instruction::ROR:
        case Instruction::RTI:
        case Instruction::SBC:
        case Instruction::SEC:
            return true;

        default:
            return false;
        }
    }

    [[nodiscard]]
    static bool WritesOverflow(
        cpu6502::Instruction instruction)
    {
        using Instruction =
            cpu6502::Instruction;

        switch (instruction)
        {
        case Instruction::ADC:
        case Instruction::BIT:
        case Instruction::CLV:
        case Instruction::PLP:
        case Instruction::RTI:
        case Instruction::SBC:
            return true;

        default:
            return false;
        }
    }

    [[nodiscard]]
    static std::vector<u16> BuildSuppressedAddresses(
        const std::vector<StructureRecord>& records,
        u16 routineAddress,
        const SemanticConditionAnalysisResult& semanticConditions)
    {
        std::vector<u16> addresses;

        for (const auto& record :
             records)
        {
            if (record.ifStatement != nullptr)
            {
                AddSuppressedCondition(
                    record.ifStatement->
                        instructionAddress,
                    routineAddress,
                    semanticConditions,
                    addresses);
            }

            if (record.loop == nullptr)
            {
                continue;
            }

            for (const auto& condition :
                 record.loop->headerConditions)
            {
                AddSuppressedCondition(
                    condition.instructionAddress,
                    routineAddress,
                    semanticConditions,
                    addresses);
            }

            for (const auto& condition :
                 record.loop->latchConditions)
            {
                AddSuppressedCondition(
                    condition.instructionAddress,
                    routineAddress,
                    semanticConditions,
                    addresses);
            }

            for (const auto& condition :
                 record.loop->bodyConditions)
            {
                AddSuppressedCondition(
                    condition.instructionAddress,
                    routineAddress,
                    semanticConditions,
                    addresses);
            }
        }

        SortUnique(
            addresses);

        return addresses;
    }

    static void AddSuppressedCondition(
        u16 branchAddress,
        u16 routineAddress,
        const SemanticConditionAnalysisResult& semanticConditions,
        std::vector<u16>& addresses)
    {
        addresses.push_back(
            branchAddress);

        const auto* semantic =
            semanticConditions.Find(
                routineAddress,
                branchAddress);

        if (semantic == nullptr ||
            !semantic->semanticResolved ||
            !semantic->producerFound)
        {
            return;
        }

        addresses.push_back(
            semantic->producerAddress);
    }

    static void AssignStatementParent(
        StatementRecord& statement,
        const std::vector<StructureRecord>& records,
        u16 blockAddress)
    {
        std::optional<ParentCandidate> best;

        for (std::size_t index = 0;
             index < records.size();
             ++index)
        {
            const auto& record =
                records[index];

            if (record.ifStatement != nullptr)
            {
                AddStatementArmCandidate(
                    record.ifStatement->thenBlocks,
                    ParentArm::Then,
                    index,
                    blockAddress,
                    best);

                AddStatementArmCandidate(
                    record.ifStatement->elseBlocks,
                    ParentArm::Else,
                    index,
                    blockAddress,
                    best);
            }

            if (record.loop != nullptr &&
                ContainsAddress(
                    record.loop->blockAddresses,
                    blockAddress))
            {
                ConsiderParent(
                    ParentCandidate{
                        index,
                        UniqueCount(
                            record.loop->
                                blockAddresses),
                        ParentArm::LoopBody,
                        false},
                    best);
            }
        }

        if (!best.has_value())
        {
            return;
        }

        statement.parentIndex =
            best->index;

        statement.parentArm =
            best->arm;
    }

    static void AddStatementArmCandidate(
        const std::vector<u16>& armBlocks,
        ParentArm arm,
        std::size_t parentIndex,
        u16 blockAddress,
        std::optional<ParentCandidate>& best)
    {
        if (!ContainsAddress(
                armBlocks,
                blockAddress))
        {
            return;
        }

        ConsiderParent(
            ParentCandidate{
                parentIndex,
                UniqueCount(
                    armBlocks),
                arm,
                false},
            best);
    }

    [[nodiscard]]
    static std::string FormatStatement(
        const DisassemblyMetadata& metadata,
        const DisassembledInstruction& instruction)
    {
        switch (instruction.instruction)
        {
        case cpu6502::Instruction::RTS:

            return
                "return";

        default:

            return
                StructuredStatementFormatter{}
                    .Format(
                        metadata,
                        instruction);
        }
    }

    [[nodiscard]]
    static std::vector<StructuredExpression> BuildChildren(
        const std::vector<StructureRecord>& records,
        const std::vector<StatementRecord>& statements,
        std::optional<std::size_t> parentIndex,
        ParentArm arm,
        u16 routineAddress,
        const SemanticConditionAnalysisResult& semanticConditions)
    {
        std::vector<StructuredExpression> children;

        for (const auto& statement :
             statements)
        {
            if (statement.parentIndex == parentIndex &&
                statement.parentArm == arm)
            {
                children.push_back(
                    statement.expression);
            }
        }

        for (std::size_t index = 0;
             index < records.size();
             ++index)
        {
            const auto& record =
                records[index];

            if (record.loweredToStatement)
            {
                continue;
            }

            if (record.parentIndex != parentIndex ||
                record.parentArm != arm)
            {
                continue;
            }

            children.push_back(
                BuildNode(
                    records,
                    statements,
                    index,
                    routineAddress,
                    semanticConditions));
        }

        SortExpressions(
            children);

        return children;
    }

    [[nodiscard]]
    static StructuredExpression BuildNode(
        const std::vector<StructureRecord>& records,
        const std::vector<StatementRecord>& statements,
        std::size_t index,
        u16 routineAddress,
        const SemanticConditionAnalysisResult& semanticConditions)
    {
        const auto& record =
            records[index];

        if (record.kind == StructureKind::If)
        {
            return
                BuildIfNode(
                    records,
                    statements,
                    index,
                    routineAddress,
                    semanticConditions);
        }

        return
            BuildLoopNode(
                records,
                statements,
                index,
                routineAddress,
                semanticConditions);
    }

    [[nodiscard]]
    static StructuredExpression BuildIfNode(
        const std::vector<StructureRecord>& records,
        const std::vector<StatementRecord>& statements,
        std::size_t index,
        u16 routineAddress,
        const SemanticConditionAnalysisResult& semanticConditions)
    {
        const auto& statement =
            *records[index].ifStatement;

        StructuredExpression expression;

        expression.kind =
            statement.HasElse()
                ? StructuredExpressionKind::IfElse
                : StructuredExpressionKind::If;

        expression.address =
            statement.instructionAddress;

        expression.condition =
            BuildCondition(
                semanticConditions,
                routineAddress,
                statement.instructionAddress,
                statement.thenState,
                statement.flag);

        expression.children =
            BuildChildren(
                records,
                statements,
                index,
                ParentArm::Then,
                routineAddress,
                semanticConditions);

        expression.elseChildren =
            BuildChildren(
                records,
                statements,
                index,
                ParentArm::Else,
                routineAddress,
                semanticConditions);

        return expression;
    }

    [[nodiscard]]
    static StructuredExpression BuildLoopNode(
        const std::vector<StructureRecord>& records,
        const std::vector<StatementRecord>& statements,
        std::size_t index,
        u16 routineAddress,
        const SemanticConditionAnalysisResult& semanticConditions)
    {
        const auto& loop =
            *records[index].loop;

        StructuredExpression expression;

        expression.kind =
            ExpressionKindForLoop(
                loop.kind);

        expression.address =
            loop.headerAddress;

        if (loop.primaryCondition.has_value())
        {
            const auto& condition =
                loop.primaryCondition.value();

            expression.condition =
                BuildCondition(
                    semanticConditions,
                    routineAddress,
                    condition.instructionAddress,
                    condition.continueState,
                    condition.flag);

            if (loop.kind ==
                StructuredLoopKind::While)
            {
                expression.address =
                    condition.instructionAddress;
            }
        }

        expression.children =
            BuildChildren(
                records,
                statements,
                index,
                ParentArm::LoopBody,
                routineAddress,
                semanticConditions);

        for (const auto& condition :
             loop.bodyConditions)
        {
            if (SubtreeContainsIfInstruction(
                    records,
                    index,
                    condition.instructionAddress))
            {
                continue;
            }

            expression.children.push_back(
                BuildBreakExpression(
                    condition,
                    routineAddress,
                    semanticConditions));
        }

        SortExpressions(
            expression.children);

        return expression;
    }

    [[nodiscard]]
    static StructuredExpression BuildBreakExpression(
        const LoopCondition& condition,
        u16 routineAddress,
        const SemanticConditionAnalysisResult& semanticConditions)
    {
        StructuredExpression expression;

        expression.kind =
            StructuredExpressionKind::If;

        expression.address =
            condition.instructionAddress;

        expression.condition =
            BuildCondition(
                semanticConditions,
                routineAddress,
                condition.instructionAddress,
                condition.exitState,
                condition.flag);

        StructuredExpression breakExpression;

        breakExpression.kind =
            StructuredExpressionKind::Break;

        breakExpression.address =
            condition.instructionAddress;

        expression.children.push_back(
            std::move(
                breakExpression));

        return expression;
    }

    [[nodiscard]]
    static std::string BuildCondition(
        const SemanticConditionAnalysisResult& semanticConditions,
        u16 routineAddress,
        u16 branchAddress,
        FlagState state,
        ProcessorFlag flag)
    {
        const auto* semantic =
            semanticConditions.Find(
                routineAddress,
                branchAddress);

        if (semantic != nullptr)
        {
            const std::string& expression =
                semantic->ExpressionForState(
                    state);

            if (!expression.empty())
            {
                return expression;
            }
        }

        std::string expression =
            FlagName(flag);

        expression +=
            state == FlagState::Set
                ? " == 1"
                : " == 0";

        return expression;
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

    [[nodiscard]]
    static StructuredExpressionKind ExpressionKindForLoop(
        StructuredLoopKind kind) noexcept
    {
        switch (kind)
        {
        case StructuredLoopKind::While:
            return
                StructuredExpressionKind::While;

        case StructuredLoopKind::DoWhile:
            return
                StructuredExpressionKind::DoWhile;

        case StructuredLoopKind::Infinite:
        case StructuredLoopKind::Complex:
        default:
            return
                StructuredExpressionKind::InfiniteLoop;
        }
    }

    [[nodiscard]]
    static bool SubtreeContainsIfInstruction(
        const std::vector<StructureRecord>& records,
        std::size_t ancestorIndex,
        u16 instructionAddress)
    {
        for (std::size_t index = 0;
             index < records.size();
             ++index)
        {
            const auto& record =
                records[index];

            if (record.kind != StructureKind::If ||
                record.ifStatement == nullptr ||
                record.ifStatement->
                    instructionAddress !=
                    instructionAddress)
            {
                continue;
            }

            std::optional<std::size_t> parent =
                record.parentIndex;

            while (parent.has_value())
            {
                if (parent.value() ==
                    ancestorIndex)
                {
                    return true;
                }

                parent =
                    records[parent.value()].
                        parentIndex;
            }
        }

        return false;
    }

    [[nodiscard]]
    static std::optional<std::size_t> FindRecord(
        const std::vector<StructureRecord>& records,
        StructureKind kind,
        u16 address)
    {
        for (std::size_t index = 0;
             index < records.size();
             ++index)
        {
            if (records[index].kind == kind &&
                records[index].address == address)
            {
                return index;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]]
    static bool ContainsAddress(
        const std::vector<u16>& addresses,
        u16 address)
    {
        return
            std::find(
                addresses.begin(),
                addresses.end(),
                address) !=
            addresses.end();
    }

    [[nodiscard]]
    static bool ContainsAll(
        const std::vector<u16>& container,
        const std::vector<u16>& values)
    {
        return std::all_of(
            values.begin(),
            values.end(),
            [&](u16 value)
            {
                return
                    std::find(
                        container.begin(),
                        container.end(),
                        value) !=
                    container.end();
            });
    }

    [[nodiscard]]
    static std::size_t UniqueCount(
        const std::vector<u16>& values)
    {
        std::vector<u16> copy =
            values;

        SortUnique(
            copy);

        return
            copy.size();
    }

    static void SortUnique(
        std::vector<u16>& values)
    {
        std::sort(
            values.begin(),
            values.end());

        values.erase(
            std::unique(
                values.begin(),
                values.end()),
            values.end());
    }

    static void SortExpressions(
        std::vector<StructuredExpression>& expressions)
    {
        std::stable_sort(
            expressions.begin(),
            expressions.end(),
            [](const StructuredExpression& left,
               const StructuredExpression& right)
            {
                return
                    left.address <
                    right.address;
            });
    }
};

} // namespace atari
