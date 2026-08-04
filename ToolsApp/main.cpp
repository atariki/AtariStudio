#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/ProjectStatistics.h>
#include <AtariStudio/Disassembler/AnalysisEngine.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Formats/XEX/XexLoader.h>

namespace
{

enum class OutputMode
{
    None,
    EmitCpp,
    EmitCppOnly
};

void PrintUsage(
    std::ostream& stream)
{
    stream
        << "Usage:\n"
        << "  TestApp <file.xex>\n"
        << "  TestApp <file.xex> --emit-cpp <file.cpp>\n"
        << "  TestApp <file.xex> --emit-cpp-only <file.cpp>\n"
        << "  TestApp --help\n";
}

bool PathsReferToSameFile(
    const std::filesystem::path& left,
    const std::filesystem::path& right)
{
    std::error_code error;

    if (std::filesystem::exists(
            left,
            error) &&
        !error &&
        std::filesystem::exists(
            right,
            error) &&
        !error)
    {
        const bool equivalent =
            std::filesystem::equivalent(
                left,
                right,
                error);

        if (!error)
        {
            return equivalent;
        }
    }

    std::error_code leftError;
    std::error_code rightError;

    const auto normalizedLeft =
        std::filesystem::weakly_canonical(
            left,
            leftError);

    const auto normalizedRight =
        std::filesystem::weakly_canonical(
            right,
            rightError);

    return
        !leftError &&
        !rightError &&
        normalizedLeft == normalizedRight;
}

std::string PathForDisplay(
    const std::filesystem::path& path)
{
    const auto utf8 =
        path.u8string();

    return std::string{
        reinterpret_cast<const char*>(
            utf8.data()),
        utf8.size()};
}

// ============================================================
// Segment helpers
// ============================================================

const char* SegmentTypeToString(
    atari::SegmentType type)
{
    switch (type)
    {
    case atari::SegmentType::Unknown:
        return "Unknown";

    case atari::SegmentType::Code:
        return "Code";

    case atari::SegmentType::Data:
        return "Data";

    case atari::SegmentType::Charset:
        return "Charset";

    case atari::SegmentType::Screen:
        return "Screen";

    case atari::SegmentType::DisplayList:
        return "DisplayList";

    case atari::SegmentType::Hardware:
        return "Hardware";

    case atari::SegmentType::ZeroPage:
        return "ZeroPage";

    case atari::SegmentType::System:
        return "System";

    default:
        return "Unknown";
    }
}

// ============================================================
// Edge helpers
// ============================================================

const char* BasicBlockEdgeTypeToString(
    atari::BasicBlockEdgeType type)
{
    switch (type)
    {
    case atari::BasicBlockEdgeType::FallThrough:
        return "fall";

    case atari::BasicBlockEdgeType::BranchTaken:
        return "branch";

    case atari::BasicBlockEdgeType::Jump:
        return "jump";

    default:
        return "?";
    }
}

const char* GraphEdgeTypeToString(
    atari::ControlFlowGraphEdgeType type)
{
    switch (type)
    {
    case atari::ControlFlowGraphEdgeType::FallThrough:
        return "fall";

    case atari::ControlFlowGraphEdgeType::BranchTaken:
        return "branch";

    case atari::ControlFlowGraphEdgeType::Jump:
        return "jump";

    default:
        return "?";
    }
}

// ============================================================
// Conditional helpers
// ============================================================

const char* ConditionalRegionKindToString(
    atari::ConditionalRegionKind kind)
{
    switch (kind)
    {
    case atari::ConditionalRegionKind::IfThen:
        return "if";

    case atari::ConditionalRegionKind::IfElse:
        return "if-else";

    default:
        return "?";
    }
}

const char* ProcessorFlagToString(
    atari::ProcessorFlag flag)
{
    switch (flag)
    {
    case atari::ProcessorFlag::Carry:
        return "C";

    case atari::ProcessorFlag::Zero:
        return "Z";

    case atari::ProcessorFlag::Negative:
        return "N";

    case atari::ProcessorFlag::Overflow:
        return "V";

    default:
        return "?";
    }
}

const char* FlagStateOperator(
    atari::FlagState state)
{
    switch (state)
    {
    case atari::FlagState::Clear:
        return "== 0";

    case atari::FlagState::Set:
        return "== 1";

    default:
        return "?";
    }
}

const char* FlagStateToString(
    atari::FlagState state)
{
    switch (state)
    {
    case atari::FlagState::Clear:
        return "clear";

    case atari::FlagState::Set:
        return "set";

    default:
        return "?";
    }
}

const char* BranchMnemonic(
    atari::cpu6502::Instruction instruction)
{
    switch (instruction)
    {
    case atari::cpu6502::Instruction::BCC:
        return "BCC";

    case atari::cpu6502::Instruction::BCS:
        return "BCS";

    case atari::cpu6502::Instruction::BEQ:
        return "BEQ";

    case atari::cpu6502::Instruction::BNE:
        return "BNE";

    case atari::cpu6502::Instruction::BMI:
        return "BMI";

    case atari::cpu6502::Instruction::BPL:
        return "BPL";

    case atari::cpu6502::Instruction::BVC:
        return "BVC";

    case atari::cpu6502::Instruction::BVS:
        return "BVS";

    default:
        return "?";
    }
}

const char* StructuredArmToString(
    atari::StructuredArm arm)
{
    switch (arm)
    {
    case atari::StructuredArm::None:
        return "none";

    case atari::StructuredArm::Then:
        return "then";

    case atari::StructuredArm::Else:
        return "else";

    default:
        return "?";
    }
}

// ============================================================
// Flag producer helpers
// ============================================================

const char* FlagProducerKindToString(
    atari::FlagProducerKind kind)
{
    switch (kind)
    {
    case atari::FlagProducerKind::ValueResult:
        return "value-result";

    case atari::FlagProducerKind::Compare:
        return "compare";

    case atari::FlagProducerKind::BitTest:
        return "bit-test";

    case atari::FlagProducerKind::ExplicitClear:
        return "explicit-clear";

    case atari::FlagProducerKind::ExplicitSet:
        return "explicit-set";

    case atari::FlagProducerKind::StatusRestore:
        return "status-restore";

    case atari::FlagProducerKind::Unknown:
        return "unknown";

    default:
        return "?";
    }
}

const char* FlagProducerStopReasonToString(
    atari::FlagProducerStopReason reason)
{
    switch (reason)
    {
    case atari::FlagProducerStopReason::None:
        return "none";

    case atari::FlagProducerStopReason::EntryBoundary:
        return "entry-boundary";

    case atari::FlagProducerStopReason::AmbiguousPredecessors:
        return "ambiguous-predecessors";

    case atari::FlagProducerStopReason::CallBarrier:
        return "call-barrier";

    case atari::FlagProducerStopReason::Cycle:
        return "cycle";

    case atari::FlagProducerStopReason::MissingBlock:
        return "missing-block";

    default:
        return "?";
    }
}

// ============================================================
// Loop helpers
// ============================================================

const char* LoopConditionPositionToString(
    atari::LoopConditionPosition position)
{
    switch (position)
    {
    case atari::LoopConditionPosition::Header:
        return "header";

    case atari::LoopConditionPosition::Latch:
        return "latch";

    case atari::LoopConditionPosition::Body:
        return "body";

    default:
        return "?";
    }
}

const char* StructuredLoopKindToString(
    atari::StructuredLoopKind kind)
{
    switch (kind)
    {
    case atari::StructuredLoopKind::While:
        return "while";

    case atari::StructuredLoopKind::DoWhile:
        return "do-while";

    case atari::StructuredLoopKind::Infinite:
        return "infinite";

    case atari::StructuredLoopKind::Complex:
        return "complex";

    default:
        return "?";
    }
}

// ============================================================
// Address helpers
// ============================================================

void PrintAddress(
    atari::u16 address)
{
    std::cout
        << '$'
        << std::uppercase
        << std::hex
        << std::right
        << std::setw(4)
        << std::setfill('0')
        << address;
}

std::string AddressToString(
    atari::u16 address)
{
    std::ostringstream stream;

    stream
        << '$'
        << std::uppercase
        << std::hex
        << std::right
        << std::setw(4)
        << std::setfill('0')
        << address;

    return stream.str();
}

void PrintAddressList(
    const std::vector<atari::u16>& addresses)
{
    if (addresses.empty())
    {
        std::cout << "none";
        return;
    }

    for (std::size_t i = 0;
         i < addresses.size();
         ++i)
    {
        if (i != 0)
        {
            std::cout << ", ";
        }

        PrintAddress(
            addresses[i]);
    }
}

const char* DisplayListInstructionKindToString(
    atari::DisplayListInstructionKind kind)
{
    switch (kind)
    {
    case atari::DisplayListInstructionKind::Blank:
        return "blank";

    case atari::DisplayListInstructionKind::ModeLine:
        return "mode";

    case atari::DisplayListInstructionKind::Jump:
        return "jmp";

    case atari::DisplayListInstructionKind::
        JumpAndWaitForVerticalBlank:
        return "jvb";

    default:
        return "?";
    }
}

const char* DisplayListStopReasonToString(
    atari::DisplayListStopReason reason)
{
    switch (reason)
    {
    case atari::DisplayListStopReason::None:
        return "none";

    case atari::DisplayListStopReason::
        JumpAndWaitForVerticalBlank:
        return "jvb";

    case atari::DisplayListStopReason::
        UninitializedMemory:
        return "uninitialized-memory";

    case atari::DisplayListStopReason::
        TruncatedInstruction:
        return "truncated-instruction";

    case atari::DisplayListStopReason::
        AddressSpaceBoundary:
        return "address-space-boundary";

    case atari::DisplayListStopReason::
        OneKilobyteBoundary:
        return "1k-boundary";

    case atari::DisplayListStopReason::LoopDetected:
        return "loop";

    case atari::DisplayListStopReason::InstructionLimit:
        return "instruction-limit";

    default:
        return "?";
    }
}

void PrintDisplayLists(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " ANTIC Display Lists\n"
        << "=====================================\n";

    if (analysis.displayLists.displayLists.empty())
    {
        std::cout << "  none\n";
        return;
    }

    for (const auto& displayList :
         analysis.displayLists.displayLists)
    {
        std::cout << "\n  LIST ";
        PrintAddress(displayList.entryPoint);

        std::cout
            << std::dec
            << "  instructions="
            << displayList.instructions.size()
            << " bytes="
            << displayList.ByteCount()
            << " stop="
            << DisplayListStopReasonToString(
                   displayList.stopReason)
            << '\n';

        for (const auto& instruction :
             displayList.instructions)
        {
            std::cout << "    ";
            PrintAddress(instruction.address);

            std::cout
                << "  $"
                << std::uppercase
                << std::hex
                << std::setw(2)
                << std::setfill('0')
                << static_cast<unsigned>(
                       instruction.opcode)
                << "  "
                << DisplayListInstructionKindToString(
                       instruction.kind);

            if (instruction.kind ==
                atari::DisplayListInstructionKind::Blank)
            {
                std::cout
                    << std::dec
                    << " lines="
                    << static_cast<unsigned>(
                           instruction.blankScanLines);
            }
            else if (instruction.kind ==
                     atari::DisplayListInstructionKind::ModeLine)
            {
                std::cout
                    << std::hex
                    << " mode=$"
                    << static_cast<unsigned>(
                           instruction.mode);

                if (instruction.memoryScanAddress.has_value())
                {
                    std::cout << " lms=";
                    PrintAddress(
                        instruction.memoryScanAddress.value());
                }

                if (instruction.horizontalScroll)
                {
                    std::cout << " hscroll";
                }

                if (instruction.verticalScroll)
                {
                    std::cout << " vscroll";
                }
            }
            else if (instruction.jumpAddress.has_value())
            {
                std::cout << " target=";
                PrintAddress(
                    instruction.jumpAddress.value());
            }

            if (instruction.displayListInterrupt)
            {
                std::cout << " dli";
            }

            if (instruction.reservedJumpModifier)
            {
                std::cout << " [reserved-jump-modifier]";
            }

            std::cout << '\n';
        }
    }

    std::cout << "\n  Screen memory: ";
    PrintAddressList(
        analysis.displayLists.screenMemoryAddresses);
    std::cout << '\n';
}

const char* CharacterSetLayoutToString(
    atari::CharacterSetLayout layout)
{
    switch (layout)
    {
    case atari::CharacterSetLayout::Characters64:
        return "64 characters / 512 bytes";

    case atari::CharacterSetLayout::Characters128:
        return "128 characters / 1024 bytes";

    default:
        return "?";
    }
}

void PrintCharacterSets(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " ANTIC Character Sets\n"
        << "=====================================\n";

    if (analysis.characterSets.characterSets.empty())
    {
        std::cout << "  none\n";
        return;
    }

    for (const auto& characterSet :
         analysis.characterSets.characterSets)
    {
        std::cout << "  SET ";
        PrintAddress(characterSet.baseAddress);

        std::cout
            << std::dec
            << "  "
            << CharacterSetLayoutToString(
                   characterSet.layout)
            << "  glyphs="
            << characterSet.glyphs.size()
            << "  initialized="
            << characterSet.initializedByteCount
            << '/'
            << characterSet.ExpectedByteCount();

        if (characterSet.Complete())
        {
            std::cout << "  [complete]";
        }
        else
        {
            std::cout << "  [incomplete]";
        }

        if (characterSet.addressSpaceTruncated)
        {
            std::cout << "  [address-space-boundary]";
        }

        std::cout << '\n';
    }
}

// ============================================================
// Relocation
// ============================================================

void PrintRelocationMap(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\nRelocation map:\n";

    const auto& relocation =
        analysis.metadata.Relocation();

    if (relocation.ranges.empty())
    {
        std::cout << "  none\n";
        return;
    }

    for (const auto& range :
         relocation.ranges)
    {
        const std::uint32_t sourceEnd =
            static_cast<std::uint32_t>(
                range.sourceBegin) +
            range.size - 1;

        const std::uint32_t destinationEnd =
            static_cast<std::uint32_t>(
                range.destinationBegin) +
            range.size - 1;

        std::cout << "  ";

        PrintAddress(
            range.sourceBegin);

        std::cout << " - ";

        PrintAddress(
            static_cast<atari::u16>(
                sourceEnd));

        std::cout << "  ->  ";

        PrintAddress(
            range.destinationBegin);

        std::cout << " - ";

        PrintAddress(
            static_cast<atari::u16>(
                destinationEnd));

        std::cout
            << std::dec
            << "  ("
            << range.size
            << " bytes)\n";
    }
}

// ============================================================
// Routines
// ============================================================

void PrintRoutines(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Routines\n"
        << "=====================================\n";

    if (analysis.routines.routines.empty())
    {
        std::cout
            << "\nNo routines detected.\n";

        return;
    }

    for (const auto& routine :
         analysis.routines.routines)
    {
        std::cout
            << "\n"
            << routine.name
            << "  ";

        PrintAddress(
            routine.entryAddress);

        std::cout << " - ";

        PrintAddress(
            routine.endAddress);

        std::cout
            << std::dec
            << "  "
            << routine.InstructionCount()
            << " instructions";

        if (routine.projectEntryPoint)
        {
            std::cout << "  [ENTRY]";
        }

        std::cout
            << "\n  callers:      ";

        PrintAddressList(
            routine.callers);

        std::cout
            << "\n  tail callers: ";

        PrintAddressList(
            routine.tailCallers);

        std::cout
            << "\n  callees:      ";

        if (routine.callees.empty())
        {
            std::cout << "none";
        }
        else
        {
            for (std::size_t i = 0;
                 i < routine.callees.size();
                 ++i)
            {
                if (i != 0)
                {
                    std::cout << ", ";
                }

                const auto& callee =
                    routine.callees[i];

                if (callee.type ==
                    atari::RoutineCalleeType::TailJump)
                {
                    std::cout << "JMP ";
                }
                else
                {
                    std::cout << "JSR ";
                }

                PrintAddress(
                    callee.targetAddress);

                if (callee.relocated)
                {
                    std::cout
                        << " [runtime ";

                    PrintAddress(
                        callee.encodedTarget);

                    std::cout << ']';
                }
            }
        }

        std::cout << '\n';
    }
}

// ============================================================
// Basic Blocks
// ============================================================

void PrintBasicBlocks(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Basic Blocks\n"
        << "=====================================\n";

    for (const auto& routine :
         analysis.basicBlocks.routines)
    {
        std::cout
            << "\n"
            << routine.routineName
            << "  ";

        PrintAddress(
            routine.routineEntryAddress);

        std::cout
            << std::dec
            << "  ("
            << routine.blocks.size()
            << " blocks)\n";

        for (const auto& block :
             routine.blocks)
        {
            std::cout << "  ";

            PrintAddress(
                block.beginAddress);

            std::cout << " - ";

            PrintAddress(
                block.endAddress);

            std::cout
                << std::dec
                << "  "
                << block.InstructionCount()
                << " instructions";

            if (block.terminal)
            {
                std::cout << "  [terminal]";
            }

            std::cout
                << "\n    -> ";

            if (block.successors.empty())
            {
                std::cout << "none";
            }
            else
            {
                for (std::size_t i = 0;
                     i < block.successors.size();
                     ++i)
                {
                    if (i != 0)
                    {
                        std::cout << ", ";
                    }

                    const auto& edge =
                        block.successors[i];

                    std::cout
                        << BasicBlockEdgeTypeToString(
                            edge.type)
                        << ' ';

                    PrintAddress(
                        edge.targetAddress);
                }
            }

            std::cout << '\n';
        }
    }
}

// ============================================================
// Control Flow Graphs
// ============================================================

void PrintControlFlowGraphs(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Control Flow Graphs\n"
        << "=====================================\n";

    for (const auto& graph :
         analysis.graphs.routines)
    {
        std::cout
            << "\n"
            << graph.routineName
            << "  ";

        PrintAddress(
            graph.routineEntryAddress);

        std::cout
            << std::dec
            << "  nodes="
            << graph.nodes.size()
            << " edges="
            << graph.edges.size()
            << '\n';

        for (const auto& node :
             graph.nodes)
        {
            std::cout << "  NODE ";

            PrintAddress(
                node.address);

            std::cout << " - ";

            PrintAddress(
                node.endAddress);

            if (node.entry)
            {
                std::cout << " [entry]";
            }

            if (node.terminal)
            {
                std::cout << " [terminal]";
            }

            std::cout
                << "\n    pred: ";

            PrintAddressList(
                node.predecessors);

            std::cout
                << "\n    succ: ";

            PrintAddressList(
                node.successors);

            std::cout << '\n';
        }

        std::cout
            << "  EDGES:\n";

        if (graph.edges.empty())
        {
            std::cout
                << "    none\n";
        }
        else
        {
            for (const auto& edge :
                 graph.edges)
            {
                std::cout << "    ";

                PrintAddress(
                    edge.sourceAddress);

                std::cout
                    << " --"
                    << GraphEdgeTypeToString(
                        edge.type)
                    << "--> ";

                PrintAddress(
                    edge.targetAddress);

                std::cout << '\n';
            }
        }
    }
}

// ============================================================
// Flag Producers
// ============================================================

void PrintFlagProducers(
    const atari::Project& project,
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Flag Producers\n"
        << "=====================================\n";

    atari::Disassembler disassembler;

    for (const auto& routine :
         analysis.flagProducers.routines)
    {
        std::cout
            << "\n"
            << routine.routineName
            << "  ";

        PrintAddress(
            routine.routineEntryAddress);

        std::cout
            << std::dec
            << "  branches="
            << routine.producers.size()
            << " found="
            << routine.FoundCount()
            << " unresolved="
            << routine.UnresolvedCount()
            << '\n';

        if (routine.producers.empty())
        {
            std::cout
                << "  none\n";

            continue;
        }

        for (const auto& producer :
             routine.producers)
        {
            std::cout
                << "\n  BRANCH ";

            PrintAddress(
                producer.branchAddress);

            std::cout
                << ' '
                << BranchMnemonic(
                    producer.branchInstruction)
                << "  flag="
                << ProcessorFlagToString(
                    producer.flag)
                << '\n';

            if (!producer.found)
            {
                std::cout
                    << "    producer: unresolved\n"
                    << "    reason:   "
                    << FlagProducerStopReasonToString(
                        producer.stopReason)
                    << '\n'
                    << "    scanned:  "
                    << std::dec
                    << producer.instructionsBack
                    << " instructions, "
                    << producer.blocksBack
                    << " predecessor blocks\n";

                continue;
            }

            const auto decodedProducer =
                disassembler.Decode(
                    project.GetMemory(),
                    producer.producerAddress);

            std::cout
                << "    producer: ";

            PrintAddress(
                producer.producerAddress);

            std::cout
                << ' '
                << decodedProducer.text
                << '\n';

            std::cout
                << "    kind:     "
                << FlagProducerKindToString(
                    producer.kind)
                << '\n';

            std::cout
                << "    location: "
                << (producer.IsLocal()
                        ? "same block"
                        : "predecessor block")
                << '\n';

            std::cout
                << "    distance: "
                << std::dec
                << producer.instructionsBack
                << " instructions, "
                << producer.blocksBack
                << " predecessor blocks\n";

            if (producer.constantState.has_value())
            {
                std::cout
                    << "    constant: "
                    << ProcessorFlagToString(
                        producer.flag)
                    << " = "
                    << FlagStateToString(
                        producer.constantState.value())
                    << '\n';
            }
        }
    }
}

// ============================================================
// Dominator Analysis
// ============================================================

void PrintDominators(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Dominator Analysis\n"
        << "=====================================\n";

    for (const auto& routine :
         analysis.dominators.routines)
    {
        const auto* graph =
            analysis.graphs.FindRoutine(
                routine.routineEntryAddress);

        std::cout << '\n';

        if (graph != nullptr)
        {
            std::cout
                << graph->routineName
                << "  ";
        }

        PrintAddress(
            routine.routineEntryAddress);

        std::cout
            << std::dec
            << "  nodes="
            << routine.nodes.size()
            << " back-edges="
            << routine.backEdges.size()
            << '\n';

        for (const auto& node :
             routine.nodes)
        {
            std::cout << "  NODE ";

            PrintAddress(
                node.address);

            std::cout
                << "\n    idom: ";

            if (node.immediateDominator.has_value())
            {
                PrintAddress(
                    node.immediateDominator.value());
            }
            else
            {
                std::cout << "none";
            }

            std::cout
                << "\n    depth: "
                << std::dec
                << node.depth
                << "\n    dom: ";

            PrintAddressList(
                node.dominators);

            std::cout << '\n';
        }

        std::cout
            << "  BACK EDGES:\n";

        if (routine.backEdges.empty())
        {
            std::cout
                << "    none\n";
        }
        else
        {
            for (const auto& edge :
                 routine.backEdges)
            {
                std::cout << "    ";

                PrintAddress(
                    edge.sourceAddress);

                std::cout << " -> ";

                PrintAddress(
                    edge.targetAddress);

                std::cout
                    << "  ["
                    << GraphEdgeTypeToString(
                        edge.type)
                    << "]\n";
            }
        }
    }
}

// ============================================================
// Post-Dominator Analysis
// ============================================================

void PrintPostDominators(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Post-Dominator Analysis\n"
        << "=====================================\n";

    for (const auto& routine :
         analysis.postDominators.routines)
    {
        std::cout
            << "\n"
            << routine.routineName
            << "  ";

        PrintAddress(
            routine.routineEntryAddress);

        std::cout
            << std::dec
            << "  nodes="
            << routine.nodes.size()
            << " exits="
            << routine.TerminalCount()
            << " iterations="
            << routine.iterations
            << " converged="
            << (routine.converged
                    ? "yes"
                    : "NO")
            << '\n';

        std::cout
            << "  exits: ";

        PrintAddressList(
            routine.terminalAddresses);

        std::cout << '\n';

        for (const auto& node :
             routine.nodes)
        {
            std::cout << "  NODE ";

            PrintAddress(
                node.address);

            if (node.terminal)
            {
                std::cout
                    << " [terminal]";
            }

            std::cout
                << "\n    ipdom: ";

            if (node.immediatePostDominator.has_value())
            {
                PrintAddress(
                    node.immediatePostDominator.value());
            }
            else
            {
                std::cout << "none";
            }

            std::cout
                << "\n    depth: "
                << std::dec
                << node.depth
                << "\n    pdom: ";

            PrintAddressList(
                node.postDominators);

            std::cout << '\n';
        }
    }
}

// ============================================================
// Conditional Regions
// ============================================================

void PrintConditionalRegions(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Conditional Regions\n"
        << "=====================================\n";

    for (const auto& routine :
         analysis.conditionals.routines)
    {
        std::cout
            << "\n"
            << routine.routineName
            << "  ";

        PrintAddress(
            routine.routineEntryAddress);

        std::cout
            << std::dec
            << "  regions="
            << routine.regions.size()
            << " if="
            << routine.IfThenCount()
            << " if-else="
            << routine.IfElseCount()
            << '\n';

        if (routine.regions.empty())
        {
            std::cout << "  none\n";
            continue;
        }

        for (const auto& region :
             routine.regions)
        {
            std::cout
                << "  "
                << ConditionalRegionKindToString(
                    region.kind)
                << " header=";

            PrintAddress(
                region.headerAddress);

            std::cout
                << " join=";

            PrintAddress(
                region.joinAddress);

            std::cout
                << "\n    branch target: ";

            PrintAddress(
                region.branchTargetAddress);

            std::cout
                << "\n    fall target:   ";

            PrintAddress(
                region.fallthroughTargetAddress);

            std::cout
                << "\n    branch blocks: ";

            PrintAddressList(
                region.branchBlocks);

            std::cout
                << "\n    fall blocks:   ";

            PrintAddressList(
                region.fallthroughBlocks);

            std::cout << '\n';
        }
    }
}

// ============================================================
// Branch Conditions
// ============================================================

void PrintBranchConditions(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Branch Conditions\n"
        << "=====================================\n";

    for (const auto& routine :
         analysis.branchConditions.routines)
    {
        std::cout
            << "\n"
            << routine.routineName
            << "  ";

        PrintAddress(
            routine.routineEntryAddress);

        std::cout
            << std::dec
            << "  conditions="
            << routine.conditions.size()
            << '\n';

        if (routine.conditions.empty())
        {
            std::cout << "  none\n";
            continue;
        }

        for (const auto& condition :
             routine.conditions)
        {
            std::cout
                << "  header=";

            PrintAddress(
                condition.headerAddress);

            std::cout
                << " instruction=";

            PrintAddress(
                condition.instructionAddress);

            std::cout
                << ' '
                << BranchMnemonic(
                    condition.instruction)
                << '\n';

            std::cout
                << "    branch taken: "
                << ProcessorFlagToString(
                    condition.flag)
                << ' '
                << FlagStateOperator(
                    condition.branchTakenState)
                << " -> ";

            PrintAddress(
                condition.branchTargetAddress);

            std::cout << '\n';

            std::cout
                << "    fall-through: "
                << ProcessorFlagToString(
                    condition.flag)
                << ' '
                << FlagStateOperator(
                    condition.fallthroughState)
                << " -> ";

            PrintAddress(
                condition.fallthroughTargetAddress);

            std::cout << '\n';

            std::cout
                << "    join: ";

            PrintAddress(
                condition.joinAddress);

            std::cout
                << "  region="
                << ConditionalRegionKindToString(
                    condition.regionKind)
                << '\n';
        }
    }
}

// ============================================================
// Structured Control Flow
// ============================================================

void PrintStructuredControlFlow(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Structured Control Flow\n"
        << "=====================================\n";

    for (const auto& routine :
         analysis.structuredControlFlow.routines)
    {
        std::cout
            << "\n"
            << routine.routineName
            << "  ";

        PrintAddress(
            routine.routineEntryAddress);

        std::cout
            << std::dec
            << "  if="
            << routine.ifStatements.size()
            << " if-else="
            << routine.IfElseCount()
            << " roots="
            << routine.RootCount()
            << " max-depth="
            << routine.MaximumDepth()
            << '\n';

        if (routine.ifStatements.empty())
        {
            std::cout << "  none\n";
            continue;
        }

        for (const auto& statement :
             routine.ifStatements)
        {
            std::cout
                << "\n  IF ";

            PrintAddress(
                statement.headerAddress);

            std::cout
                << " depth="
                << std::dec
                << statement.depth;

            if (statement.HasElse())
            {
                std::cout << " [if-else]";
            }
            else
            {
                std::cout << " [if]";
            }

            std::cout << '\n';

            std::cout
                << "    source: ";

            PrintAddress(
                statement.instructionAddress);

            std::cout
                << ' '
                << BranchMnemonic(
                    statement.sourceInstruction);

            if (statement.branchConditionInverted)
            {
                std::cout << " [inverted]";
            }

            std::cout << '\n';

            std::cout
                << "    condition: "
                << ProcessorFlagToString(
                    statement.flag)
                << ' '
                << FlagStateOperator(
                    statement.thenState)
                << '\n';

            std::cout
                << "    THEN entry: ";

            PrintAddress(
                statement.thenEntryAddress);

            std::cout
                << "\n    THEN blocks: ";

            PrintAddressList(
                statement.thenBlocks);

            std::cout
                << "\n    ELSE entry: ";

            if (statement.elseEntryAddress.has_value())
            {
                PrintAddress(
                    statement.elseEntryAddress.value());
            }
            else
            {
                std::cout << "none";
            }

            std::cout
                << "\n    ELSE blocks: ";

            PrintAddressList(
                statement.elseBlocks);

            std::cout
                << "\n    JOIN: ";

            PrintAddress(
                statement.joinAddress);

            std::cout
                << "\n    parent: ";

            if (statement.parentHeaderAddress.has_value())
            {
                PrintAddress(
                    statement.parentHeaderAddress.value());

                std::cout
                    << " ["
                    << StructuredArmToString(
                        statement.parentArm)
                    << ']';
            }
            else
            {
                std::cout << "none";
            }

            std::cout
                << "\n    children: ";

            PrintAddressList(
                statement.childHeaders);

            std::cout << '\n';
        }
    }
}

// ============================================================
// Natural Loops
// ============================================================

void PrintNaturalLoops(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Natural Loops\n"
        << "=====================================\n";

    for (const auto& routine :
         analysis.loops.routines)
    {
        std::cout
            << "\n"
            << routine.routineName
            << "  ";

        PrintAddress(
            routine.routineEntryAddress);

        std::cout
            << std::dec
            << "  loops="
            << routine.loops.size()
            << '\n';

        if (routine.loops.empty())
        {
            std::cout << "  none\n";
            continue;
        }

        for (const auto& loop :
             routine.loops)
        {
            std::cout
                << "  LOOP header=";

            PrintAddress(
                loop.headerAddress);

            std::cout
                << std::dec
                << " blocks="
                << loop.BlockCount()
                << " latches="
                << loop.LatchCount()
                << " exits="
                << loop.ExitCount();

            if (loop.IsSelfLoop())
            {
                std::cout << " [self]";
            }

            std::cout
                << "\n    latch: ";

            PrintAddressList(
                loop.latchAddresses);

            std::cout
                << "\n    blocks: ";

            PrintAddressList(
                loop.blockAddresses);

            std::cout
                << "\n    exits: ";

            if (loop.exits.empty())
            {
                std::cout << "none";
            }
            else
            {
                for (std::size_t i = 0;
                     i < loop.exits.size();
                     ++i)
                {
                    if (i != 0)
                    {
                        std::cout << ", ";
                    }

                    const auto& exit =
                        loop.exits[i];

                    PrintAddress(
                        exit.sourceAddress);

                    std::cout
                        << " --"
                        << GraphEdgeTypeToString(
                            exit.type)
                        << "--> ";

                    PrintAddress(
                        exit.targetAddress);
                }
            }

            std::cout << '\n';
        }
    }
}

// ============================================================
// Loop Conditions
// ============================================================

void PrintLoopConditions(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Loop Conditions\n"
        << "=====================================\n";

    for (const auto& routine :
         analysis.loopConditions.routines)
    {
        std::cout
            << "\n"
            << routine.routineName
            << "  ";

        PrintAddress(
            routine.routineEntryAddress);

        std::cout
            << std::dec
            << "  conditions="
            << routine.conditions.size()
            << '\n';

        if (routine.conditions.empty())
        {
            std::cout << "  none\n";
            continue;
        }

        for (const auto& condition :
             routine.conditions)
        {
            std::cout
                << "\n  LOOP ";

            PrintAddress(
                condition.loopHeaderAddress);

            std::cout
                << "  source=";

            PrintAddress(
                condition.sourceBlockAddress);

            std::cout
                << "  position="
                << LoopConditionPositionToString(
                    condition.position)
                << '\n';

            std::cout
                << "    instruction: ";

            PrintAddress(
                condition.instructionAddress);

            std::cout
                << ' '
                << BranchMnemonic(
                    condition.instruction)
                << '\n';

            std::cout
                << "    branch taken: "
                << ProcessorFlagToString(
                    condition.flag)
                << ' '
                << FlagStateOperator(
                    condition.branchTakenState)
                << " -> ";

            PrintAddress(
                condition.branchTargetAddress);

            if (condition.branchTakenContinues)
            {
                std::cout << "  [continue]";
            }
            else
            {
                std::cout << "  [exit]";
            }

            std::cout << '\n';

            std::cout
                << "    fall-through: "
                << ProcessorFlagToString(
                    condition.flag)
                << ' '
                << FlagStateOperator(
                    condition.fallthroughState)
                << " -> ";

            PrintAddress(
                condition.fallthroughTargetAddress);

            if (condition.branchTakenContinues)
            {
                std::cout << "  [exit]";
            }
            else
            {
                std::cout << "  [continue]";
            }

            std::cout << '\n';

            std::cout
                << "    continue condition: "
                << ProcessorFlagToString(
                    condition.flag)
                << ' '
                << FlagStateOperator(
                    condition.continueState)
                << " -> ";

            PrintAddress(
                condition.continueTargetAddress);

            std::cout << '\n';

            std::cout
                << "    exit condition:     "
                << ProcessorFlagToString(
                    condition.flag)
                << ' '
                << FlagStateOperator(
                    condition.exitState)
                << " -> ";

            PrintAddress(
                condition.exitTargetAddress);

            std::cout << '\n';
        }
    }
}

// ============================================================
// Loop Nesting
// ============================================================

void PrintLoopNesting(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Loop Nesting\n"
        << "=====================================\n";

    for (const auto& routine :
         analysis.loopNesting.routines)
    {
        std::cout
            << "\n"
            << routine.routineName
            << "  ";

        PrintAddress(
            routine.routineEntryAddress);

        std::cout
            << std::dec
            << "  loops="
            << routine.nodes.size()
            << " roots="
            << routine.RootCount()
            << " max-depth="
            << routine.MaximumDepth()
            << '\n';

        if (routine.nodes.empty())
        {
            std::cout << "  none\n";
            continue;
        }

        for (const auto& node :
             routine.nodes)
        {
            std::cout
                << "  LOOP ";

            PrintAddress(
                node.headerAddress);

            std::cout
                << std::dec
                << " depth="
                << node.depth;

            if (node.selfLoop)
            {
                std::cout << " [self]";
            }

            std::cout
                << "\n    parent: ";

            if (node.parentHeader.has_value())
            {
                PrintAddress(
                    node.parentHeader.value());
            }
            else
            {
                std::cout << "none";
            }

            std::cout
                << "\n    children: ";

            PrintAddressList(
                node.childHeaders);

            std::cout
                << "\n    blocks: "
                << std::dec
                << node.blockCount
                << "  latches: "
                << node.latchCount
                << "  exits: "
                << node.exitCount
                << '\n';
        }
    }
}

// ============================================================
// Structured Loops
// ============================================================

void PrintStructuredLoops(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Structured Loops\n"
        << "=====================================\n";

    for (const auto& routine :
         analysis.loopStructures.routines)
    {
        std::cout
            << "\n"
            << routine.routineName
            << "  ";

        PrintAddress(
            routine.routineEntryAddress);

        std::cout
            << std::dec
            << "  loops="
            << routine.loops.size()
            << " while="
            << routine.WhileCount()
            << " do-while="
            << routine.DoWhileCount()
            << " infinite="
            << routine.InfiniteCount()
            << " complex="
            << routine.ComplexCount()
            << " breaks="
            << routine.BreakConditionCount()
            << '\n';

        if (routine.loops.empty())
        {
            std::cout << "  none\n";
            continue;
        }

        for (const auto& loop :
             routine.loops)
        {
            std::cout
                << "\n  LOOP ";

            PrintAddress(
                loop.headerAddress);

            std::cout
                << "  kind="
                << StructuredLoopKindToString(
                    loop.kind)
                << "  depth="
                << std::dec
                << loop.depth;

            if (loop.selfLoop)
            {
                std::cout << "  [self]";
            }

            std::cout << '\n';

            std::cout
                << "    parent: ";

            if (loop.parentHeaderAddress.has_value())
            {
                PrintAddress(
                    loop.parentHeaderAddress.value());
            }
            else
            {
                std::cout << "none";
            }

            std::cout
                << "\n    children: ";

            PrintAddressList(
                loop.childHeaders);

            std::cout
                << "\n    blocks: ";

            PrintAddressList(
                loop.blockAddresses);

            std::cout
                << "\n    latches: ";

            PrintAddressList(
                loop.latchAddresses);

            std::cout
                << "\n    exits: ";

            PrintAddressList(
                loop.exitAddresses);

            std::cout << '\n';

            if (loop.primaryCondition.has_value())
            {
                const auto& condition =
                    loop.primaryCondition.value();

                std::cout
                    << "    primary condition:\n"
                    << "      position: "
                    << LoopConditionPositionToString(
                        condition.position)
                    << '\n'
                    << "      source:   ";

                PrintAddress(
                    condition.sourceBlockAddress);

                std::cout
                    << "\n      branch:   ";

                PrintAddress(
                    condition.instructionAddress);

                std::cout
                    << ' '
                    << BranchMnemonic(
                        condition.instruction)
                    << '\n'
                    << "      continue: "
                    << ProcessorFlagToString(
                        condition.flag)
                    << ' '
                    << FlagStateOperator(
                        condition.continueState)
                    << " -> ";

                PrintAddress(
                    condition.continueTargetAddress);

                std::cout
                    << "\n      exit:     "
                    << ProcessorFlagToString(
                        condition.flag)
                    << ' '
                    << FlagStateOperator(
                        condition.exitState)
                    << " -> ";

                PrintAddress(
                    condition.exitTargetAddress);

                std::cout << '\n';
            }
            else
            {
                std::cout
                    << "    primary condition: none\n";
            }

            std::cout
                << "    header conditions: "
                << std::dec
                << loop.headerConditions.size()
                << '\n';

            for (const auto& condition :
                 loop.headerConditions)
            {
                std::cout << "      ";

                PrintAddress(
                    condition.instructionAddress);

                std::cout
                    << ' '
                    << BranchMnemonic(
                        condition.instruction)
                    << "  continue when "
                    << ProcessorFlagToString(
                        condition.flag)
                    << ' '
                    << FlagStateOperator(
                        condition.continueState)
                    << "  exit ";

                PrintAddress(
                    condition.exitTargetAddress);

                std::cout << '\n';
            }

            std::cout
                << "    latch conditions: "
                << std::dec
                << loop.latchConditions.size()
                << '\n';

            for (const auto& condition :
                 loop.latchConditions)
            {
                std::cout << "      ";

                PrintAddress(
                    condition.instructionAddress);

                std::cout
                    << ' '
                    << BranchMnemonic(
                        condition.instruction)
                    << "  continue when "
                    << ProcessorFlagToString(
                        condition.flag)
                    << ' '
                    << FlagStateOperator(
                        condition.continueState)
                    << "  exit ";

                PrintAddress(
                    condition.exitTargetAddress);

                std::cout << '\n';
            }

            std::cout
                << "    body conditions: "
                << std::dec
                << loop.bodyConditions.size()
                << '\n';

            for (const auto& condition :
                 loop.bodyConditions)
            {
                std::cout
                    << "      BREAK-LIKE ";

                PrintAddress(
                    condition.instructionAddress);

                std::cout
                    << ' '
                    << BranchMnemonic(
                        condition.instruction)
                    << '\n'
                    << "        continue when "
                    << ProcessorFlagToString(
                        condition.flag)
                    << ' '
                    << FlagStateOperator(
                        condition.continueState)
                    << " -> ";

                PrintAddress(
                    condition.continueTargetAddress);

                std::cout
                    << "\n        break when    "
                    << ProcessorFlagToString(
                        condition.flag)
                    << ' '
                    << FlagStateOperator(
                        condition.exitState)
                    << " -> ";

                PrintAddress(
                    condition.exitTargetAddress);

                std::cout << '\n';
            }

            std::cout
                << "    reconstructed:\n";

            switch (loop.kind)
            {
            case atari::StructuredLoopKind::While:

                if (loop.primaryCondition.has_value())
                {
                    const auto& condition =
                        loop.primaryCondition.value();

                    std::cout
                        << "      while ("
                        << ProcessorFlagToString(
                            condition.flag)
                        << ' '
                        << FlagStateOperator(
                            condition.continueState)
                        << ")\n"
                        << "      {\n"
                        << "          ...\n"
                        << "      }\n";
                }

                break;

            case atari::StructuredLoopKind::DoWhile:

                std::cout
                    << "      do\n"
                    << "      {\n"
                    << "          ...\n"
                    << "      }\n";

                if (loop.primaryCondition.has_value())
                {
                    const auto& condition =
                        loop.primaryCondition.value();

                    std::cout
                        << "      while ("
                        << ProcessorFlagToString(
                            condition.flag)
                        << ' '
                        << FlagStateOperator(
                            condition.continueState)
                        << ");\n";
                }

                break;

            case atari::StructuredLoopKind::Infinite:

                std::cout
                    << "      for (;;)\n"
                    << "      {\n";

                for (const auto& condition :
                     loop.bodyConditions)
                {
                    std::cout
                        << "          if ("
                        << ProcessorFlagToString(
                            condition.flag)
                        << ' '
                        << FlagStateOperator(
                            condition.exitState)
                        << ")\n"
                        << "          {\n"
                        << "              break;\n"
                        << "          }\n";
                }

                if (loop.bodyConditions.empty())
                {
                    std::cout
                        << "          ...\n";
                }

                std::cout
                    << "      }\n";

                break;

            case atari::StructuredLoopKind::Complex:

                std::cout
                    << "      /* complex loop */\n"
                    << "      for (;;)\n"
                    << "      {\n"
                    << "          /* header conditions: "
                    << loop.headerConditions.size()
                    << " */\n"
                    << "          /* body exit conditions: "
                    << loop.bodyConditions.size()
                    << " */\n"
                    << "          /* latch conditions: "
                    << loop.latchConditions.size()
                    << " */\n"
                    << "      }\n";

                break;

            default:

                std::cout
                    << "      unknown\n";

                break;
            }
        }
    }
}

// ============================================================
// Structured pseudo-C
// ============================================================

void PrintStructuredCode(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Structured Pseudo-C\n"
        << "=====================================\n";

    const auto& code =
        analysis.structured.GeneratedCode();

    if (code.empty())
    {
        std::cout
            << "\nNo structured code generated.\n";

        return;
    }

    std::cout
        << '\n'
        << code;
}

// ============================================================
// Listing
// ============================================================

void PrintCodeRow(
    const atari::DisassemblyListingRow& row)
{
    std::cout
        << std::left
        << std::setw(12)
        << std::setfill(' ')
        << row.label;

    PrintAddress(
        row.address);

    std::cout << "  ";

    for (const atari::u8 value :
         row.bytes)
    {
        std::cout
            << std::uppercase
            << std::hex
            << std::right
            << std::setw(2)
            << std::setfill('0')
            << static_cast<unsigned>(
                value)
            << ' ';
    }

    for (std::size_t i =
             row.bytes.size();
         i < 3;
         ++i)
    {
        std::cout << "   ";
    }

    std::cout
        << ' '
        << std::left
        << std::setw(24)
        << std::setfill(' ')
        << row.instruction;

    if (!row.comment.empty())
    {
        std::cout
            << " ; "
            << row.comment;
    }

    std::cout << '\n';
}

void PrintDataRow(
    const atari::DisassemblyListingRow& row)
{
    std::cout
        << std::left
        << std::setw(12)
        << std::setfill(' ')
        << row.label;

    PrintAddress(
        row.address);

    std::cout << "  ";

    for (const atari::u8 value :
         row.bytes)
    {
        std::cout
            << std::uppercase
            << std::hex
            << std::right
            << std::setw(2)
            << std::setfill('0')
            << static_cast<unsigned>(
                value)
            << ' ';
    }

    std::cout << '\n';
}

void PrintListing(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Code / Data Listing\n"
        << "=====================================\n\n"
        << "LABEL       ADDRESS  BYTES       INSTRUCTION\n"
        << "---------------------------------------------------------------------\n";

    for (const auto& region :
         analysis.listing.Regions())
    {
        std::cout << '\n';

        if (region.IsCode())
        {
            std::cout
                << "; CODE "
                << AddressToString(
                    region.begin)
                << " - "
                << AddressToString(
                    region.end)
                << '\n';
        }
        else
        {
            std::cout
                << "; DATA "
                << AddressToString(
                    region.begin)
                << " - "
                << AddressToString(
                    region.end)
                << " ("
                << std::dec
                << region.Size()
                << " bytes)\n";
        }

        for (const auto& row :
             region.rows)
        {
            if (row.IsCode())
            {
                PrintCodeRow(
                    row);
            }
            else
            {
                PrintDataRow(
                    row);
            }
        }
    }
}

} // namespace

// ============================================================
// main
// ============================================================

#ifdef _WIN32
int wmain(
    int argc,
    wchar_t* argv[])
#else
int main(
    int argc,
    char* argv[])
#endif
{
    std::cout
        << "=====================================\n"
        << " AtariStudio Test Application\n"
        << "=====================================\n\n";

    if (argc == 2)
    {
        const std::filesystem::path firstArgument{
            argv[1]};

        if (firstArgument ==
                std::filesystem::path{"--help"} ||
            firstArgument ==
                std::filesystem::path{"-h"})
        {
            PrintUsage(std::cout);
            return 0;
        }
    }

    if (argc < 2)
    {
        PrintUsage(std::cerr);
        return 2;
    }

    OutputMode outputMode =
        OutputMode::None;

    std::filesystem::path outputFilename;

    if (argc >= 3)
    {
        const std::filesystem::path option{
            argv[2]};

        if (option ==
            std::filesystem::path{"--emit-cpp"})
        {
            outputMode =
                OutputMode::EmitCpp;
        }
        else if (option ==
                 std::filesystem::path{
                     "--emit-cpp-only"})
        {
            outputMode =
                OutputMode::EmitCppOnly;
        }
        else
        {
            std::cerr
                << "Unknown option: "
                << PathForDisplay(option)
                << '\n';

            PrintUsage(std::cerr);
            return 2;
        }

        if (argc < 4)
        {
            std::cerr
                << "Missing output filename for "
                << PathForDisplay(option)
                << ".\n";

            PrintUsage(std::cerr);
            return 2;
        }

        if (argc > 4)
        {
            std::cerr
                << "Unexpected extra argument: "
                << PathForDisplay(
                    std::filesystem::path{
                        argv[4]})
                << '\n';

            PrintUsage(std::cerr);
            return 2;
        }

        outputFilename =
            argv[3];
    }

    const std::filesystem::path filename =
        argv[1];

    if (outputMode != OutputMode::None &&
        PathsReferToSameFile(
            filename,
            outputFilename))
    {
        std::cerr
            << "Input and output filenames refer "
            << "to the same file.\n";

        return 1;
    }

    auto project =
        std::make_unique<atari::Project>();

    atari::XexLoader loader;

    std::cout
        << "Loading XEX:\n"
        << PathForDisplay(filename)
        << "\n\n";

    if (!loader.Load(
            filename,
            *project))
    {
        std::cerr
            << "XEX load failed.\n"
            << "Error: "
            << loader.LastError()
            << '\n';

        return 1;
    }

    atari::AnalysisEngine
        analysisEngine;

    const auto analysis =
        analysisEngine.Analyze(
            *project);

    if (outputMode != OutputMode::None)
    {
        std::ofstream output(
            outputFilename,
            std::ios::binary |
                std::ios::trunc);

        if (!output)
        {
            std::cerr
                << "Cannot create translation unit: "
                << PathForDisplay(outputFilename)
                << '\n';

            return 1;
        }

        output
            << analysis.structured.
                GeneratedTranslationUnit();

        if (!output)
        {
            std::cerr
                << "Cannot write translation unit: "
                << PathForDisplay(outputFilename)
                << '\n';

            return 1;
        }

        std::cout
            << "Generated C++ translation unit: "
            << PathForDisplay(outputFilename)
            << "\n\n";

        if (outputMode ==
            OutputMode::EmitCppOnly)
        {
            return 0;
        }
    }

    std::cout
        << "XEX loaded successfully.\n\n";

    // ========================================================
    // Segments
    // ========================================================

    const auto& segments =
        project->Segments();

    std::cout
        << "Segments:\n\n";

    for (std::size_t i = 0;
         i < segments.size();
         ++i)
    {
        const auto& segment =
            segments[i];

        std::cout
            << "  ["
            << std::dec
            << i
            << "] ";

        PrintAddress(
            segment.begin);

        std::cout << " - ";

        PrintAddress(
            segment.end);

        std::cout
            << "  "
            << SegmentTypeToString(
                segment.type)
            << "  "
            << std::dec
            << segment.Size()
            << " bytes";

        if (!segment.name.empty())
        {
            std::cout
                << "  "
                << segment.name;
        }

        if (segment.overlapping)
        {
            std::cout
                << "  [OVERLAP]";
        }

        std::cout << '\n';
    }

    // ========================================================
    // RUN / INIT
    // ========================================================

    std::cout
        << "\nRUN address:  ";

    if (project->RunAddress() != 0)
    {
        PrintAddress(
            project->RunAddress());
    }
    else
    {
        std::cout << "not set";
    }

    std::cout
        << "\nINIT address: ";

    if (project->InitAddress() != 0)
    {
        PrintAddress(
            project->InitAddress());
    }
    else
    {
        std::cout << "not set";
    }

    std::cout << '\n';

    // ========================================================
    // XEX statistics
    // ========================================================

    const auto statistics =
        atari::CalculateProjectStatistics(
            *project);

    std::cout
        << std::dec
        << "\nXEX Statistics:\n"
        << "  Segments:     "
        << statistics.segmentCount
        << '\n'
        << "  Code:         "
        << statistics.codeSegments
        << '\n'
        << "  Data:         "
        << statistics.dataSegments
        << '\n'
        << "  System:       "
        << statistics.systemSegments
        << '\n'
        << "  Unknown:      "
        << statistics.unknownSegments
        << '\n'
        << "  Charset:      "
        << statistics.charsetSegments
        << '\n'
        << "  Screen:       "
        << statistics.screenSegments
        << '\n'
        << "  DisplayList:  "
        << statistics.displayListSegments
        << '\n'
        << "  Hardware:     "
        << statistics.hardwareSegments
        << '\n'
        << "  ZeroPage:     "
        << statistics.zeroPageSegments
        << '\n'
        << "  Overlapping:  "
        << statistics.overlappingSegments
        << '\n'
        << "  Total bytes:  "
        << statistics.totalBytes
        << '\n';

    // ========================================================
    // Analysis statistics
    // ========================================================

    std::cout
        << std::dec
        << "\nAnalysis:\n"
        << "  Entry points:             "
        << analysis.controlFlow.entryPoints.size()
        << '\n'
        << "  CFG instructions:         "
        << analysis.cfgInstructionCount
        << '\n'
        << "  Code-island instructions: "
        << analysis.codeIslandInstructionCount
        << '\n'
        << "  Total instructions:       "
        << analysis.TotalInstructionCount()
        << '\n'
        << "  ANTIC display lists:      "
        << analysis.DisplayListCount()
        << '\n'
        << "  Complete display lists:   "
        << analysis.CompleteDisplayListCount()
        << '\n'
        << "  Display-list instructions:"
        << ' '
        << analysis.DisplayListInstructionCount()
        << '\n'
        << "  Screen-memory references: "
        << analysis.displayLists.
               screenMemoryAddresses.size()
        << '\n'
        << "  Character sets:           "
        << analysis.CharacterSetCount()
        << '\n'
        << "  Complete character sets:  "
        << analysis.CompleteCharacterSetCount()
        << '\n'
        << "  Character glyphs:         "
        << analysis.CharacterGlyphCount()
        << '\n'
        << "  Cross references:         "
        << analysis.CrossReferenceCount()
        << '\n'
        << "  Symbols:                  "
        << analysis.SymbolCount()
        << '\n'
        << "  Code/Data regions:        "
        << analysis.regions.size()
        << '\n'
        << "  Routines:                 "
        << analysis.RoutineCount()
        << '\n'
        << "  Basic blocks:             "
        << analysis.BasicBlockCount()
        << '\n'
        << "  CFG nodes:                "
        << analysis.GraphNodeCount()
        << '\n'
        << "  CFG edges:                "
        << analysis.GraphEdgeCount()
        << '\n'
        << "  Flag branches:            "
        << analysis.FlagProducerBranchCount()
        << '\n'
        << "  Flag producers found:     "
        << analysis.FlagProducerFoundCount()
        << '\n'
        << "  Flag producers unresolved:"
        << ' '
        << analysis.FlagProducerUnresolvedCount()
        << '\n'
        << "  Dominator nodes:          "
        << analysis.DominatorNodeCount()
        << '\n'
        << "  Back edges:               "
        << analysis.BackEdgeCount()
        << '\n'
        << "  Post-dominator nodes:     "
        << analysis.PostDominatorNodeCount()
        << '\n'
        << "  Post-dominator exits:     "
        << analysis.PostDominatorTerminalCount()
        << '\n'
        << "  Conditional regions:      "
        << analysis.ConditionalRegionCount()
        << '\n'
        << "  Branch conditions:        "
        << analysis.BranchConditionCount()
        << '\n'
        << "  If regions:               "
        << analysis.IfThenCount()
        << '\n'
        << "  If-else regions:          "
        << analysis.IfElseCount()
        << '\n'
        << "  Structured IFs:           "
        << analysis.StructuredIfCount()
        << '\n'
        << "  Structured IF-ELSEs:      "
        << analysis.StructuredIfElseCount()
        << '\n'
        << "  Structured roots:         "
        << analysis.StructuredRootCount()
        << '\n'
        << "  Structured max depth:     "
        << analysis.StructuredMaximumDepth()
        << '\n'
        << "  Natural loops:            "
        << analysis.NaturalLoopCount()
        << '\n'
        << "  Loop exits:               "
        << analysis.LoopExitCount()
        << '\n'
        << "  Loop conditions:          "
        << analysis.LoopConditionCount()
        << '\n'
        << "  Loop tree nodes:          "
        << analysis.LoopTreeNodeCount()
        << '\n'
        << "  Root loops:               "
        << analysis.RootLoopCount()
        << '\n'
        << "  Maximum loop depth:       "
        << analysis.MaximumLoopDepth()
        << '\n'
        << "  Structured loops:         "
        << analysis.StructuredLoopCount()
        << '\n'
        << "  Structured while:         "
        << analysis.StructuredWhileCount()
        << '\n'
        << "  Structured do-while:      "
        << analysis.StructuredDoWhileCount()
        << '\n'
        << "  Structured infinite:      "
        << analysis.StructuredInfiniteLoopCount()
        << '\n'
        << "  Structured complex:       "
        << analysis.StructuredComplexLoopCount()
        << '\n'
        << "  Structured break tests:   "
        << analysis.StructuredBreakConditionCount()
        << '\n'
        << "  Structured loop depth:    "
        << analysis.StructuredLoopMaximumDepth()
        << '\n'
        << "  Structured expressions:   "
        << analysis.StructuredExpressionCount()
        << '\n'
        << "  Structured statements:    "
        << analysis.StructuredStatementCount()
        << '\n'
        << "  Structured code bytes:    "
        << analysis.structured.GeneratedCode().size()
        << '\n'
        << "  Listing rows:             "
        << analysis.ListingRowCount()
        << '\n';

    // ========================================================
    // Detailed reports
    // ========================================================

    PrintRelocationMap(
        analysis);

    PrintDisplayLists(
        analysis);

    PrintCharacterSets(
        analysis);

    PrintRoutines(
        analysis);

    PrintBasicBlocks(
        analysis);

    PrintControlFlowGraphs(
        analysis);

    PrintFlagProducers(
        *project,
        analysis);

    PrintDominators(
        analysis);

    PrintPostDominators(
        analysis);

    PrintConditionalRegions(
        analysis);

    PrintBranchConditions(
        analysis);

    PrintStructuredControlFlow(
        analysis);

    PrintNaturalLoops(
        analysis);

    PrintLoopConditions(
        analysis);

    PrintLoopNesting(
        analysis);

    PrintStructuredLoops(
        analysis);

    PrintStructuredCode(
        analysis);

    PrintListing(
        analysis);

    //
    // TestApp must terminate automatically.
    //
    // Do not add std::cin.get().
    //
    return 0;
}
