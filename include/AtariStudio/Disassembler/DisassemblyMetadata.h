#pragma once

#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <AtariStudio/Atari/AtariSymbols.h>
#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Cpu6502/AddressMode.h>
#include <AtariStudio/Cpu6502/Instruction.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>
#include <AtariStudio/Disassembler/CrossReferenceAnalyzer.h>
#include <AtariStudio/Disassembler/DisassembledInstruction.h>
#include <AtariStudio/Disassembler/SymbolTable.h>

namespace atari
{

class DisassemblyMetadata
{
public:

    void Build(
        const Project& project,
        const ControlFlowAnalysisResult& analysis)
    {
        //
        // Symbols.
        //
        m_symbols.Build(
            project,
            analysis);

        //
        // Cross references.
        //
        CrossReferenceAnalyzer
            crossReferenceAnalyzer;

        m_crossReferences =
            crossReferenceAnalyzer.Analyze(
                project.GetMemory(),
                analysis);

        //
        // Save relocation information.
        //
        m_relocation =
            analysis.relocation;
    }

    [[nodiscard]]
    const SymbolTable&
    Symbols() const noexcept
    {
        return m_symbols;
    }

    [[nodiscard]]
    const CrossReferenceAnalysisResult&
    CrossReferences() const noexcept
    {
        return m_crossReferences;
    }

    [[nodiscard]]
    const RelocationAnalysisResult&
    Relocation() const noexcept
    {
        return m_relocation;
    }

    //
    // Converts:
    //
    // BNE $0509
    //
    // into:
    //
    // BNE L0509
    //
    // when a symbol exists.
    //
    [[nodiscard]]
    std::string FormatInstruction(
        const DisassembledInstruction&
            instruction) const
    {
        u16 target = 0;
        bool hasTarget = false;

        switch (instruction.instruction)
        {
        case cpu6502::Instruction::BCC:
        case cpu6502::Instruction::BCS:
        case cpu6502::Instruction::BEQ:
        case cpu6502::Instruction::BMI:
        case cpu6502::Instruction::BNE:
        case cpu6502::Instruction::BPL:
        case cpu6502::Instruction::BVC:
        case cpu6502::Instruction::BVS:

            target =
                RelativeTarget(
                    instruction);

            hasTarget = true;
            break;

        case cpu6502::Instruction::JSR:

            target =
                AbsoluteTarget(
                    instruction);

            hasTarget = true;
            break;

        case cpu6502::Instruction::JMP:

            if (instruction.addressMode ==
                cpu6502::AddressMode::Absolute)
            {
                target =
                    AbsoluteTarget(
                        instruction);

                hasTarget = true;
            }

            break;

        default:
            break;
        }

        if (!hasTarget)
        {
            return instruction.text;
        }

        const std::string* symbol =
            m_symbols.Find(
                target);

        if (symbol == nullptr)
        {
            return instruction.text;
        }

        return
            instruction.mnemonic +
            " " +
            *symbol;
    }

    //
    // Builds everything printed after ';':
    //
    // NMIEN
    // runtime $0767
    // -> L0567
    // XREF: JSR $052F, JSR $0545
    //
    [[nodiscard]]
    std::string BuildComment(
        const DisassembledInstruction&
            instruction) const
    {
        std::vector<std::string>
            comments;

        //
        // Atari OS / hardware symbol.
        //
        const std::string_view
            atariComment =
                AtariComment(
                    instruction);

        if (!atariComment.empty())
        {
            comments.emplace_back(
                atariComment);
        }

        //
        // Relocated control-flow target.
        //
        const std::string
            relocationTarget =
                RelocationTargetComment(
                    instruction);

        if (!relocationTarget.empty())
        {
            comments.push_back(
                relocationTarget);
        }

        //
        // Runtime address of this source
        // instruction.
        //
        const std::string
            runtime =
                RuntimeAddressComment(
                    instruction);

        if (!runtime.empty())
        {
            comments.push_back(
                runtime);
        }

        //
        // Incoming references.
        //
        const std::string
            xrefs =
                CrossReferenceComment(
                    instruction.address);

        if (!xrefs.empty())
        {
            comments.push_back(
                xrefs);
        }

        if (comments.empty())
        {
            return {};
        }

        std::ostringstream result;

        for (std::size_t i = 0;
             i < comments.size();
             ++i)
        {
            if (i != 0)
            {
                result << " | ";
            }

            result
                << comments[i];
        }

        return result.str();
    }

private:

    [[nodiscard]]
    static u16 AbsoluteTarget(
        const DisassembledInstruction&
            instruction)
    {
        return static_cast<u16>(
            static_cast<u16>(
                instruction.bytes[1]) |
            (static_cast<u16>(
                instruction.bytes[2])
             << 8));
    }

    [[nodiscard]]
    static u16 RelativeTarget(
        const DisassembledInstruction&
            instruction)
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
    static std::string AddressToString(
        u16 address)
    {
        std::ostringstream stream;

        stream
            << '$'
            << std::uppercase
            << std::hex
            << std::setw(4)
            << std::setfill('0')
            << address;

        return stream.str();
    }

    [[nodiscard]]
    static const char*
    CrossReferenceTypeToString(
        CrossReferenceType type)
    {
        switch (type)
        {
        case CrossReferenceType::Branch:
            return "BR";

        case CrossReferenceType::Call:
            return "JSR";

        case CrossReferenceType::Jump:
            return "JMP";

        default:
            return "?";
        }
    }

    [[nodiscard]]
    static std::optional<u16>
    ReferencedAddress(
        const DisassembledInstruction&
            instruction)
    {
        using AddressMode =
            cpu6502::AddressMode;

        switch (instruction.addressMode)
        {
        case AddressMode::ZeroPage:
        case AddressMode::ZeroPageX:
        case AddressMode::ZeroPageY:
        case AddressMode::IndexedIndirect:
        case AddressMode::IndirectIndexed:

            return static_cast<u16>(
                instruction.bytes[1]);

        case AddressMode::Absolute:
        case AddressMode::AbsoluteX:
        case AddressMode::AbsoluteY:
        case AddressMode::Indirect:

            return AbsoluteTarget(
                instruction);

        case AddressMode::Implied:
        case AddressMode::Accumulator:
        case AddressMode::Immediate:
        case AddressMode::Relative:
        default:

            return std::nullopt;
        }
    }

    [[nodiscard]]
    static std::string_view AtariComment(
        const DisassembledInstruction&
            instruction)
    {
        const auto address =
            ReferencedAddress(
                instruction);

        if (!address.has_value())
        {
            return {};
        }

        return AtariSymbols::Find(
            address.value());
    }

    [[nodiscard]]
    std::string RelocationTargetComment(
        const DisassembledInstruction&
            instruction) const
    {
        bool controlFlowTarget = false;

        switch (instruction.instruction)
        {
        case cpu6502::Instruction::JSR:

            controlFlowTarget = true;
            break;

        case cpu6502::Instruction::JMP:

            controlFlowTarget =
                instruction.addressMode ==
                cpu6502::AddressMode::Absolute;

            break;

        default:
            break;
        }

        if (!controlFlowTarget)
        {
            return {};
        }

        const u16 runtimeTarget =
            AbsoluteTarget(
                instruction);

        const auto source =
            m_relocation.ResolveDestination(
                runtimeTarget);

        if (!source.has_value())
        {
            return {};
        }

        std::string result =
            "-> ";

        const std::string* symbol =
            m_symbols.Find(
                source.value());

        if (symbol != nullptr)
        {
            result += *symbol;
        }
        else
        {
            result +=
                AddressToString(
                    source.value());
        }

        return result;
    }

    [[nodiscard]]
    std::string RuntimeAddressComment(
        const DisassembledInstruction&
            instruction) const
    {
        //
        // Runtime address is displayed only
        // for named positions.
        //
        if (!m_symbols.Contains(
                instruction.address))
        {
            return {};
        }

        const auto runtime =
            m_relocation.ResolveSource(
                instruction.address);

        if (!runtime.has_value())
        {
            return {};
        }

        return
            "runtime " +
            AddressToString(
                runtime.value());
    }

    [[nodiscard]]
    std::string CrossReferenceComment(
        u16 targetAddress) const
    {
        std::ostringstream stream;

        bool found = false;

        for (const auto& reference :
             m_crossReferences.references)
        {
            if (reference.targetAddress !=
                targetAddress)
            {
                continue;
            }

            if (!found)
            {
                stream << "XREF: ";

                found = true;
            }
            else
            {
                stream << ", ";
            }

            stream
                << CrossReferenceTypeToString(
                    reference.type)
                << ' '
                << AddressToString(
                    reference.sourceAddress);
        }

        if (!found)
        {
            return {};
        }

        return stream.str();
    }

    SymbolTable m_symbols;

    CrossReferenceAnalysisResult
        m_crossReferences;

    RelocationAnalysisResult
        m_relocation;
};

} // namespace atari