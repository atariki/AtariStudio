#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/ProjectStatistics.h>
#include <AtariStudio/Disassembler/AnalysisEngine.h>
#include <AtariStudio/Formats/XEX/XexLoader.h>

namespace
{

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
        std::cout
            << "  none\n";

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
            std::cout
                << "  [ENTRY]";
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
            std::cout
                << "none";
        }
        else
        {
            for (std::size_t i = 0;
                 i < routine.callees.size();
                 ++i)
            {
                const auto& callee =
                    routine.callees[i];

                if (i != 0)
                {
                    std::cout
                        << ", ";
                }

                if (callee.type ==
                    atari::RoutineCalleeType::
                        TailJump)
                {
                    std::cout
                        << "JMP ";
                }
                else
                {
                    std::cout
                        << "JSR ";
                }

                PrintAddress(
                    callee.targetAddress);

                if (callee.relocated)
                {
                    std::cout
                        << " [runtime ";

                    PrintAddress(
                        callee.encodedTarget);

                    std::cout
                        << ']';
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

    if (analysis.basicBlocks.routines.empty())
    {
        std::cout
            << "\nNo basic blocks detected.\n";

        return;
    }

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
            std::cout
                << "  ";

            PrintAddress(
                block.beginAddress);

            std::cout
                << " - ";

            PrintAddress(
                block.endAddress);

            std::cout
                << std::dec
                << "  "
                << block.InstructionCount()
                << " instructions";

            if (block.terminal)
            {
                std::cout
                    << "  [terminal]";
            }

            std::cout
                << "\n    -> ";

            if (block.successors.empty())
            {
                std::cout
                    << "none";
            }
            else
            {
                for (std::size_t i = 0;
                     i < block.successors.size();
                     ++i)
                {
                    const auto& edge =
                        block.successors[i];

                    if (i != 0)
                    {
                        std::cout
                            << ", ";
                    }

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

    if (analysis.graphs.routines.empty())
    {
        std::cout
            << "\nNo control flow graphs detected.\n";

        return;
    }

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
            std::cout
                << "  NODE ";

            PrintAddress(
                node.address);

            std::cout
                << " - ";

            PrintAddress(
                node.endAddress);

            if (node.entry)
            {
                std::cout
                    << " [entry]";
            }

            if (node.terminal)
            {
                std::cout
                    << " [terminal]";
            }

            std::cout
                << "\n    pred: ";

            PrintAddressList(
                node.predecessors);

            std::cout
                << "\n    succ: ";

            PrintAddressList(
                node.successors);

            std::cout
                << '\n';
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
                std::cout
                    << "    ";

                PrintAddress(
                    edge.sourceAddress);

                std::cout
                    << " --"
                    << GraphEdgeTypeToString(
                        edge.type)
                    << "--> ";

                PrintAddress(
                    edge.targetAddress);

                std::cout
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

    if (analysis.dominators.routines.empty())
    {
        std::cout
            << "\nNo dominator data.\n";

        return;
    }

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
            std::cout
                << "  NODE ";

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
                std::cout
                    << "none";
            }

            std::cout
                << "\n    depth: "
                << std::dec
                << node.depth
                << "\n    dom: ";

            PrintAddressList(
                node.dominators);

            std::cout
                << '\n';
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
                std::cout
                    << "    ";

                PrintAddress(
                    edge.sourceAddress);

                std::cout
                    << " -> ";

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

    std::cout
        << "  ";

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
        std::cout
            << "   ";
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

    std::cout
        << '\n';
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

    std::cout
        << "  ";

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

    std::cout
        << '\n';
}

void PrintListing(
    const atari::AnalysisEngineResult& analysis)
{
    std::cout
        << "\n=====================================\n"
        << " Code / Data Listing\n"
        << "=====================================\n\n";

    std::cout
        << "LABEL       ADDRESS  BYTES       INSTRUCTION\n"
        << "---------------------------------------------------------------------\n";

    for (const auto& region :
         analysis.listing.Regions())
    {
        std::cout
            << '\n';

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
                << " bytes)"
                << '\n';
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

int main(
    int argc,
    char* argv[])
{
    std::cout
        << "=====================================\n"
        << " AtariStudio Test Application\n"
        << "=====================================\n\n";

    if (argc < 2)
    {
        std::cout
            << "Usage:\n"
            << "  TestApp <file.xex>\n";

        return 1;
    }

    const std::filesystem::path filename =
        argv[1];

    atari::Project project;

    atari::XexLoader loader;

    std::cout
        << "Loading XEX:\n"
        << filename.string()
        << "\n\n";

    if (!loader.Load(
            filename,
            project))
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
            project);

    std::cout
        << "XEX loaded successfully.\n\n";

    // ========================================================
    // Segments
    // ========================================================

    const auto& segments =
        project.Segments();

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

        std::cout
            << " - ";

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

        std::cout
            << '\n';
    }

    // ========================================================
    // RUN / INIT
    // ========================================================

    std::cout
        << "\nRUN address:  ";

    if (project.RunAddress() != 0)
    {
        PrintAddress(
            project.RunAddress());
    }
    else
    {
        std::cout
            << "not set";
    }

    std::cout
        << "\nINIT address: ";

    if (project.InitAddress() != 0)
    {
        PrintAddress(
            project.InitAddress());
    }
    else
    {
        std::cout
            << "not set";
    }

    std::cout
        << '\n';

    // ========================================================
    // Project statistics
    // ========================================================

    const auto statistics =
        atari::CalculateProjectStatistics(
            project);

    //
    // PrintAddress() leaves std::cout in HEX mode.
    // All counters below must be decimal.
    //
    std::cout
        << std::dec;

    std::cout
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
        << std::dec;

    std::cout
        << "\nAnalysis:\n"
        << "  Entry points:             "
        << analysis.controlFlow.
            entryPoints.size()
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
        << "  Dominator nodes:          "
        << analysis.DominatorNodeCount()
        << '\n'
        << "  Back edges:               "
        << analysis.BackEdgeCount()
        << '\n'
        << "  Listing rows:             "
        << analysis.ListingRowCount()
        << '\n';

    // ========================================================
    // Detailed output
    // ========================================================

    PrintRelocationMap(
        analysis);

    PrintRoutines(
        analysis);

    PrintBasicBlocks(
        analysis);

    PrintControlFlowGraphs(
        analysis);

    PrintDominators(
        analysis);

    PrintListing(
        analysis);

    //
    // Important:
    //
    // Do NOT add std::cin.get() here.
    //
    // TestApp must terminate automatically.
    // Otherwise TestApp.exe remains locked and MSVC linker
    // produces LNK1168 on the next build.
    //
    return 0;
}