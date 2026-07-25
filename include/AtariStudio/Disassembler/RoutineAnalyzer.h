#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/CodeDataAnalyzer.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/Disassembler.h>
#include <AtariStudio/Disassembler/DisassemblyMetadata.h>

namespace atari
{

enum class RoutineCalleeType
{
    Call,
    TailJump
};

struct RoutineCallee
{
    //
    // Address of JSR/JMP instruction.
    //
    u16 sourceAddress = 0;

    //
    // Logical target address in source memory.
    //
    u16 targetAddress = 0;

    //
    // Address physically encoded in the instruction.
    //
    u16 encodedTarget = 0;

    RoutineCalleeType type =
        RoutineCalleeType::Call;

    bool relocated = false;
};

struct Routine
{
    std::string name;

    u16 entryAddress = 0;
    u16 endAddress = 0;

    //
    // Instructions belonging to this routine.
    //
    // Overlapping routines are allowed because
    // 6502 code may use shared entries/tails.
    //
    std::vector<u16> instructionAddresses;

    //
    // JSR instructions entering this routine.
    //
    std::vector<u16> callers;

    //
    // JMP instructions entering this routine.
    //
    std::vector<u16> tailCallers;

    //
    // Calls and tail jumps made by this routine.
    //
    std::vector<RoutineCallee> callees;

    bool projectEntryPoint = false;

    [[nodiscard]]
    std::size_t InstructionCount() const noexcept
    {
        return instructionAddresses.size();
    }

    [[nodiscard]]
    std::uint32_t Size() const noexcept
    {
        if (endAddress < entryAddress)
        {
            return 0;
        }

        return
            static_cast<std::uint32_t>(
                endAddress) -
            static_cast<std::uint32_t>(
                entryAddress) +
            1;
    }
};

struct RoutineAnalysisResult
{
    std::vector<Routine> routines;

    [[nodiscard]]
    const Routine* Find(
        u16 entryAddress) const noexcept
    {
        const auto iterator =
            std::find_if(
                routines.begin(),
                routines.end(),
                [entryAddress](
                    const Routine& routine)
                {
                    return
                        routine.entryAddress ==
                        entryAddress;
                });

        if (iterator ==
            routines.end())
        {
            return nullptr;
        }

        return &*iterator;
    }
};

class RoutineAnalyzer
{
public:

    [[nodiscard]]
    RoutineAnalysisResult Analyze(
        const Project& project,
        const ControlFlowAnalysisResult& controlFlow,
        const DisassemblyMetadata& metadata,
        const std::vector<CodeDataRegion>& regions) const
    {
        RoutineAnalysisResult result;

        std::vector<u16> entries;

        //
        // =================================================
        // Phase 1:
        // Strong routine entry points
        // =================================================
        //

        //
        // RUNAD.
        //
        AddEntry(
            entries,
            controlFlow,
            project.RunAddress());

        //
        // INITAD.
        //
        AddEntry(
            entries,
            controlFlow,
            project.InitAddress());

        //
        // Exact beginnings of CODE regions.
        //
        // IMPORTANT:
        //
        // Do NOT search for the nearest instruction after
        // region.begin.
        //
        // Overlapping XEX segments may begin in the middle
        // of a machine instruction.
        //
        // Example:
        //
        // $0512  9D 00 08   STA $0800,X
        //
        // An overlapping segment may begin at $0514.
        //
        // The next real instruction is $0515, but $0515
        // must NOT become a new routine merely because the
        // segment started in the previous instruction.
        //
        for (const auto& region :
             regions)
        {
            if (region.type !=
                CodeDataRegionType::Code)
            {
                continue;
            }

            //
            // AddEntry() accepts the address only when
            // region.begin is itself a known instruction.
            //
            AddEntry(
                entries,
                controlFlow,
                region.begin);
        }

        //
        // Every internal JSR destination is a strong
        // routine entry.
        //
        for (const auto& reference :
             metadata.CrossReferences().
                 references)
        {
            if (reference.type !=
                CrossReferenceType::Call)
            {
                continue;
            }

            AddEntry(
                entries,
                controlFlow,
                reference.targetAddress);
        }

        SortUnique(
            entries);

        //
        // =================================================
        // Phase 2:
        // Sequential routines
        // =================================================
        //
        // Example:
        //
        // routine1:
        //     ...
        //     RTS
        //
        // routine2:
        //     ...
        //
        // However:
        //
        //     BNE L1000
        //     RTS
        //
        // L1000:
        //
        // L1000 is still part of the previous CFG.
        //
        // Therefore the instruction following a hard
        // termination is accepted only when it has no
        // incoming branch/call/jump reference.
        //
        AddSequentialRoutineEntries(
            project,
            controlFlow,
            metadata,
            entries);

        SortUnique(
            entries);

        //
        // =================================================
        // Phase 3:
        // Build routines
        // =================================================
        //
        for (const u16 entry :
             entries)
        {
            Routine routine =
                BuildRoutine(
                    project,
                    controlFlow,
                    metadata,
                    entries,
                    entry);

            if (routine.
                    instructionAddresses.empty())
            {
                continue;
            }

            result.routines.push_back(
                std::move(routine));
        }

        //
        // =================================================
        // Phase 4:
        // Incoming JSR / JMP references
        // =================================================
        //
        for (auto& routine :
             result.routines)
        {
            for (const auto& reference :
                 metadata.CrossReferences().
                     references)
            {
                if (reference.targetAddress !=
                    routine.entryAddress)
                {
                    continue;
                }

                switch (reference.type)
                {
                case CrossReferenceType::Call:

                    routine.callers.push_back(
                        reference.sourceAddress);

                    break;

                case CrossReferenceType::Jump:

                    routine.tailCallers.push_back(
                        reference.sourceAddress);

                    break;

                case CrossReferenceType::Branch:
                default:

                    break;
                }
            }

            SortUnique(
                routine.callers);

            SortUnique(
                routine.tailCallers);
        }

        std::sort(
            result.routines.begin(),
            result.routines.end(),
            [](const Routine& left,
               const Routine& right)
            {
                return
                    left.entryAddress <
                    right.entryAddress;
            });

        return result;
    }

private:

    static void AddSequentialRoutineEntries(
        const Project& project,
        const ControlFlowAnalysisResult& controlFlow,
        const DisassemblyMetadata& metadata,
        std::vector<u16>& entries)
    {
        const auto& addresses =
            controlFlow.instructionAddresses;

        if (addresses.size() < 2)
        {
            return;
        }

        const auto& memory =
            project.GetMemory();

        Disassembler disassembler;

        for (std::size_t i = 0;
             i + 1 < addresses.size();
             ++i)
        {
            const u16 address =
                addresses[i];

            const u16 candidate =
                addresses[i + 1];

            const auto instruction =
                disassembler.Decode(
                    memory,
                    address);

            bool hardBoundary = false;

            switch (instruction.instruction)
            {
            case cpu6502::Instruction::RTS:
            case cpu6502::Instruction::RTI:
            case cpu6502::Instruction::BRK:

                hardBoundary = true;
                break;

            case cpu6502::Instruction::JMP:

                hardBoundary =
                    IsExternalJump(
                        controlFlow,
                        instruction);

                break;

            default:

                break;
            }

            if (!hardBoundary)
            {
                continue;
            }

            //
            // Already known as RUN/INIT,
            // CODE-region start or JSR target.
            //
            if (IsRoutineEntry(
                    entries,
                    candidate))
            {
                continue;
            }

            //
            // A branch/call/jump already reaches this
            // address, so it is part of an existing CFG
            // rather than simply "new code after RTS".
            //
            if (HasIncomingReference(
                    metadata,
                    candidate))
            {
                continue;
            }

            AddEntry(
                entries,
                controlFlow,
                candidate);
        }
    }

    [[nodiscard]]
    static bool HasIncomingReference(
        const DisassemblyMetadata& metadata,
        u16 address)
    {
        for (const auto& reference :
             metadata.CrossReferences().
                 references)
        {
            if (reference.targetAddress ==
                address)
            {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]]
    static bool IsKnownInstruction(
        const ControlFlowAnalysisResult& controlFlow,
        u16 address)
    {
        return std::binary_search(
            controlFlow.
                instructionAddresses.begin(),
            controlFlow.
                instructionAddresses.end(),
            address);
    }

    static void AddEntry(
        std::vector<u16>& entries,
        const ControlFlowAnalysisResult& controlFlow,
        u16 address)
    {
        if (address == 0)
        {
            return;
        }

        if (!IsKnownInstruction(
                controlFlow,
                address))
        {
            return;
        }

        entries.push_back(
            address);
    }

    [[nodiscard]]
    static bool IsRoutineEntry(
        const std::vector<u16>& entries,
        u16 address)
    {
        return std::binary_search(
            entries.begin(),
            entries.end(),
            address);
    }

    [[nodiscard]]
    static u16 AbsoluteTarget(
        const DisassembledInstruction& instruction)
    {
        return static_cast<u16>(
            static_cast<u16>(
                instruction.bytes[1]) |
            (static_cast<u16>(
                instruction.bytes[2]) << 8));
    }

    [[nodiscard]]
    static u16 RelativeTarget(
        const DisassembledInstruction& instruction)
    {
        const auto offset =
            static_cast<std::int8_t>(
                instruction.bytes[1]);

        const std::int32_t target =
            static_cast<std::int32_t>(
                instruction.address) +
            static_cast<std::int32_t>(
                instruction.length) +
            static_cast<std::int32_t>(
                offset);

        return static_cast<u16>(
            target);
    }

    [[nodiscard]]
    static u16 ResolveTarget(
        const ControlFlowAnalysisResult& controlFlow,
        u16 encodedTarget,
        bool& relocated)
    {
        relocated = false;

        const auto source =
            controlFlow.relocation.
                ResolveDestination(
                    encodedTarget);

        if (!source.has_value())
        {
            return encodedTarget;
        }

        relocated = true;

        return source.value();
    }

    [[nodiscard]]
    static bool IsExternalJump(
        const ControlFlowAnalysisResult& controlFlow,
        const DisassembledInstruction& instruction)
    {
        if (instruction.addressMode ==
            cpu6502::AddressMode::Indirect)
        {
            return true;
        }

        if (instruction.addressMode !=
            cpu6502::AddressMode::Absolute)
        {
            return true;
        }

        bool relocated = false;

        const u16 target =
            ResolveTarget(
                controlFlow,
                AbsoluteTarget(
                    instruction),
                relocated);

        return
            !IsKnownInstruction(
                controlFlow,
                target);
    }

    [[nodiscard]]
    static std::string MakeRoutineName(
        const DisassemblyMetadata& metadata,
        u16 entryAddress)
    {
        if (const std::string* symbol =
                metadata.Symbols().Find(
                    entryAddress);
            symbol != nullptr)
        {
            return *symbol;
        }

        std::ostringstream stream;

        stream
            << "SUB_"
            << std::uppercase
            << std::hex
            << std::setw(4)
            << std::setfill('0')
            << entryAddress;

        return stream.str();
    }

    [[nodiscard]]
    static bool IsProjectEntry(
        const Project& project,
        u16 address) noexcept
    {
        return
            address == project.RunAddress() ||
            address == project.InitAddress();
    }

    [[nodiscard]]
    Routine BuildRoutine(
        const Project& project,
        const ControlFlowAnalysisResult& controlFlow,
        const DisassemblyMetadata& metadata,
        const std::vector<u16>& routineEntries,
        u16 entryAddress) const
    {
        Routine routine;

        routine.entryAddress =
            entryAddress;

        routine.endAddress =
            entryAddress;

        routine.name =
            MakeRoutineName(
                metadata,
                entryAddress);

        routine.projectEntryPoint =
            IsProjectEntry(
                project,
                entryAddress);

        const auto& memory =
            project.GetMemory();

        Disassembler disassembler;

        std::array<bool, MemorySize>
            visited{};

        std::deque<u16>
            workList;

        workList.push_back(
            entryAddress);

        constexpr std::size_t
            MaximumInstructions = 4096;

        const auto enqueue =
            [&](u16 address)
            {
                if (!IsKnownInstruction(
                        controlFlow,
                        address))
                {
                    return;
                }

                if (visited[address])
                {
                    return;
                }

                workList.push_back(
                    address);
            };

        while (!workList.empty())
        {
            const u16 address =
                workList.front();

            workList.pop_front();

            if (visited[address])
            {
                continue;
            }

            if (!IsKnownInstruction(
                    controlFlow,
                    address))
            {
                continue;
            }

            visited[address] = true;

            const auto instruction =
                disassembler.Decode(
                    memory,
                    address);

            if (instruction.length == 0 ||
                instruction.length > 3)
            {
                continue;
            }

            routine.instructionAddresses.
                push_back(
                    address);

            const std::uint32_t end32 =
                static_cast<std::uint32_t>(
                    address) +
                instruction.length - 1;

            if (end32 <= 0xFFFF)
            {
                routine.endAddress =
                    std::max(
                        routine.endAddress,
                        static_cast<u16>(
                            end32));
            }

            if (routine.
                    instructionAddresses.size() >
                MaximumInstructions)
            {
                break;
            }

            const std::uint32_t next32 =
                static_cast<std::uint32_t>(
                    address) +
                instruction.length;

            const auto enqueueNext =
                [&]()
                {
                    if (next32 <= 0xFFFF)
                    {
                        enqueue(
                            static_cast<u16>(
                                next32));
                    }
                };

            switch (instruction.instruction)
            {
            //
            // Conditional branch:
            // target and fall-through belong to the
            // current routine.
            //
            case cpu6502::Instruction::BCC:
            case cpu6502::Instruction::BCS:
            case cpu6502::Instruction::BEQ:
            case cpu6502::Instruction::BMI:
            case cpu6502::Instruction::BNE:
            case cpu6502::Instruction::BPL:
            case cpu6502::Instruction::BVC:
            case cpu6502::Instruction::BVS:
            {
                enqueue(
                    RelativeTarget(
                        instruction));

                enqueueNext();

                break;
            }

            //
            // JSR:
            // record callee but do not descend into it.
            //
            case cpu6502::Instruction::JSR:
            {
                const u16 encodedTarget =
                    AbsoluteTarget(
                        instruction);

                bool relocated = false;

                const u16 target =
                    ResolveTarget(
                        controlFlow,
                        encodedTarget,
                        relocated);

                RoutineCallee callee;

                callee.sourceAddress =
                    address;

                callee.targetAddress =
                    target;

                callee.encodedTarget =
                    encodedTarget;

                callee.type =
                    RoutineCalleeType::Call;

                callee.relocated =
                    relocated;

                routine.callees.push_back(
                    callee);

                enqueueNext();

                break;
            }

            //
            // JMP:
            //
            // - another routine entry -> tail call
            // - internal target -> continue CFG
            // - external target -> terminate path
            //
            case cpu6502::Instruction::JMP:
            {
                if (instruction.addressMode !=
                    cpu6502::AddressMode::Absolute)
                {
                    break;
                }

                const u16 encodedTarget =
                    AbsoluteTarget(
                        instruction);

                bool relocated = false;

                const u16 target =
                    ResolveTarget(
                        controlFlow,
                        encodedTarget,
                        relocated);

                if (target != entryAddress &&
                    IsRoutineEntry(
                        routineEntries,
                        target))
                {
                    RoutineCallee callee;

                    callee.sourceAddress =
                        address;

                    callee.targetAddress =
                        target;

                    callee.encodedTarget =
                        encodedTarget;

                    callee.type =
                        RoutineCalleeType::TailJump;

                    callee.relocated =
                        relocated;

                    routine.callees.push_back(
                        callee);

                    break;
                }

                if (IsKnownInstruction(
                        controlFlow,
                        target))
                {
                    enqueue(
                        target);
                }

                break;
            }

            //
            // End of execution path.
            //
            case cpu6502::Instruction::RTS:
            case cpu6502::Instruction::RTI:
            case cpu6502::Instruction::BRK:
            case cpu6502::Instruction::Illegal:

                break;

            default:

                enqueueNext();

                break;
            }
        }

        SortUnique(
            routine.instructionAddresses);

        std::sort(
            routine.callees.begin(),
            routine.callees.end(),
            [](const RoutineCallee& left,
               const RoutineCallee& right)
            {
                if (left.targetAddress !=
                    right.targetAddress)
                {
                    return
                        left.targetAddress <
                        right.targetAddress;
                }

                if (left.sourceAddress !=
                    right.sourceAddress)
                {
                    return
                        left.sourceAddress <
                        right.sourceAddress;
                }

                return
                    static_cast<int>(
                        left.type) <
                    static_cast<int>(
                        right.type);
            });

        routine.callees.erase(
            std::unique(
                routine.callees.begin(),
                routine.callees.end(),
                [](const RoutineCallee& left,
                   const RoutineCallee& right)
                {
                    return
                        left.sourceAddress ==
                            right.sourceAddress &&
                        left.targetAddress ==
                            right.targetAddress &&
                        left.encodedTarget ==
                            right.encodedTarget &&
                        left.type ==
                            right.type;
                }),
            routine.callees.end());

        return routine;
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
};

} // namespace atari