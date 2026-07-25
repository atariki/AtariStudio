#pragma once

#include <iomanip>
#include <map>
#include <sstream>
#include <string>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/ControlFlowAnalyzer.h>

namespace atari
{

class SymbolTable
{
public:

    void Clear()
    {
        m_symbols.clear();
    }

    void Build(
        const Project& project,
        const ControlFlowAnalysisResult& analysis)
    {
        Clear();

        //
        // Automatic labels generated from
        // branch / JSR / JMP targets.
        //
        for (const u16 address :
             analysis.targetAddresses)
        {
            m_symbols[address] =
                MakeAutomaticLabel(
                    address);
        }

        //
        // RUNAD has highest priority.
        //
        if (project.RunAddress() != 0)
        {
            m_symbols[
                project.RunAddress()] =
                    "RUN_ENTRY";
        }

        //
        // INITAD receives its own name
        // unless it is identical to RUNAD.
        //
        if (project.InitAddress() != 0 &&
            project.InitAddress() !=
                project.RunAddress())
        {
            m_symbols[
                project.InitAddress()] =
                    "INIT_ENTRY";
        }
    }

    [[nodiscard]]
    const std::string* Find(
        u16 address) const noexcept
    {
        const auto iterator =
            m_symbols.find(
                address);

        if (iterator ==
            m_symbols.end())
        {
            return nullptr;
        }

        return &iterator->second;
    }

    [[nodiscard]]
    bool Contains(
        u16 address) const noexcept
    {
        return
            m_symbols.find(address) !=
            m_symbols.end();
    }

    [[nodiscard]]
    std::size_t Size() const noexcept
    {
        return m_symbols.size();
    }

    [[nodiscard]]
    const std::map<u16, std::string>&
    Symbols() const noexcept
    {
        return m_symbols;
    }

private:

    [[nodiscard]]
    static std::string MakeAutomaticLabel(
        u16 address)
    {
        std::ostringstream stream;

        stream
            << 'L'
            << std::uppercase
            << std::hex
            << std::setw(4)
            << std::setfill('0')
            << address;

        return stream.str();
    }

    std::map<u16, std::string>
        m_symbols;
};

} // namespace atari