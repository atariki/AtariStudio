#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include <AtariStudio/Core/Memory.h>
#include <AtariStudio/Core/Types.h>

namespace atari
{

struct RelocationRange
{
    u16 sourceBegin = 0;
    u16 destinationBegin = 0;

    std::uint32_t size = 0;

    [[nodiscard]]
    bool ContainsDestination(
        u16 address) const noexcept
    {
        const std::uint32_t value =
            address;

        const std::uint32_t begin =
            destinationBegin;

        const std::uint32_t end =
            begin + size;

        return
            value >= begin &&
            value < end;
    }

    [[nodiscard]]
    std::optional<u16> DestinationToSource(
        u16 address) const noexcept
    {
        if (!ContainsDestination(address))
        {
            return std::nullopt;
        }

        const std::uint32_t offset =
            static_cast<std::uint32_t>(address) -
            static_cast<std::uint32_t>(
                destinationBegin);

        const std::uint32_t source =
            static_cast<std::uint32_t>(
                sourceBegin) +
            offset;

        if (source > 0xFFFF)
        {
            return std::nullopt;
        }

        return static_cast<u16>(
            source);
    }
};

struct RelocationAnalysisResult
{
    std::vector<RelocationRange> ranges;

    [[nodiscard]]
    std::optional<u16> ResolveDestination(
        u16 address) const noexcept
    {
        for (auto iterator = ranges.rbegin();
             iterator != ranges.rend();
             ++iterator)
        {
            const auto source =
                iterator->DestinationToSource(
                    address);

            if (source.has_value())
            {
                return source;
            }
        }

        return std::nullopt;
    }
};

class RelocationAnalyzer
{
public:

    [[nodiscard]]
    RelocationAnalysisResult Analyze(
        const Memory& memory) const
    {
        RelocationAnalysisResult result;

        //
        // Ищем типичный 6502 copy loop:
        //
        // LDX #$00
        //
        // loop:
        //     ...
        //     LDA source,X
        //     STA destination,X
        //     ...
        //     INX
        //     BNE loop
        //
        // После переполнения X копируется
        // ровно 256 байт.
        //

        for (std::uint32_t address = 2;
             address <= 0xFFFC;
             ++address)
        {
            const auto incrementAddress =
                static_cast<u16>(address);

            if (!memory.Cell(
                    incrementAddress).initialized)
            {
                continue;
            }

            //
            // INX
            //
            if (memory.Read8(
                    incrementAddress) != 0xE8)
            {
                continue;
            }

            const auto branchAddress =
                static_cast<u16>(
                    address + 1);

            if (!memory.Cell(
                    branchAddress).initialized ||
                !memory.Cell(
                    static_cast<u16>(
                        address + 2)).initialized)
            {
                continue;
            }

            //
            // BNE relative
            //
            if (memory.Read8(
                    branchAddress) != 0xD0)
            {
                continue;
            }

            const auto offset =
                static_cast<std::int8_t>(
                    memory.Read8(
                        static_cast<u16>(
                            address + 2)));

            const std::int32_t branchTargetValue =
                static_cast<std::int32_t>(
                    address + 3) +
                static_cast<std::int32_t>(
                    offset);

            if (branchTargetValue < 0 ||
                branchTargetValue >
                    static_cast<std::int32_t>(
                        address))
            {
                continue;
            }

            const auto branchTarget =
                static_cast<u16>(
                    branchTargetValue);

            //
            // Не рассматриваем слишком большие
            // циклы как простой copy loop.
            //
            if (address -
                    static_cast<std::uint32_t>(
                        branchTarget) >
                64)
            {
                continue;
            }

            //
            // Проверяем наличие LDX #$00
            // непосредственно перед циклом
            // или внутри его начала.
            //
            bool initializesX = false;

            const std::uint32_t searchBegin =
                branchTarget >= 8
                    ? branchTarget - 8
                    : 0;

            for (std::uint32_t candidate =
                     searchBegin;
                 candidate <=
                     static_cast<std::uint32_t>(
                         branchTarget);
                 ++candidate)
            {
                if (candidate > 0xFFFE)
                {
                    break;
                }

                const auto candidateAddress =
                    static_cast<u16>(
                        candidate);

                const auto nextAddress =
                    static_cast<u16>(
                        candidate + 1);

                if (!memory.Cell(
                        candidateAddress).initialized ||
                    !memory.Cell(
                        nextAddress).initialized)
                {
                    continue;
                }

                //
                // LDX #$00
                //
                if (memory.Read8(
                        candidateAddress) ==
                        0xA2 &&
                    memory.Read8(
                        nextAddress) ==
                        0x00)
                {
                    initializesX = true;
                    break;
                }
            }

            if (!initializesX)
            {
                continue;
            }

            //
            // Теперь внутри цикла ищем:
            //
            // BD xx xx   LDA absolute,X
            // 9D xx xx   STA absolute,X
            //
            for (std::uint32_t candidate =
                     branchTarget;
                 candidate + 5 <= address;
                 ++candidate)
            {
                const auto candidateAddress =
                    static_cast<u16>(
                        candidate);

                bool complete = true;

                for (std::uint32_t i = 0;
                     i < 6;
                     ++i)
                {
                    const auto byteAddress =
                        static_cast<u16>(
                            candidate + i);

                    if (!memory.Cell(
                            byteAddress).initialized)
                    {
                        complete = false;
                        break;
                    }
                }

                if (!complete)
                {
                    continue;
                }

                //
                // LDA absolute,X
                //
                if (memory.Read8(
                        candidateAddress) !=
                    0xBD)
                {
                    continue;
                }

                //
                // STA absolute,X
                //
                if (memory.Read8(
                        static_cast<u16>(
                            candidate + 3)) !=
                    0x9D)
                {
                    continue;
                }

                const u16 source =
                    static_cast<u16>(
                        static_cast<u16>(
                            memory.Read8(
                                static_cast<u16>(
                                    candidate + 1))) |
                        (static_cast<u16>(
                            memory.Read8(
                                static_cast<u16>(
                                    candidate + 2)))
                         << 8));

                const u16 destination =
                    static_cast<u16>(
                        static_cast<u16>(
                            memory.Read8(
                                static_cast<u16>(
                                    candidate + 4))) |
                        (static_cast<u16>(
                            memory.Read8(
                                static_cast<u16>(
                                    candidate + 5)))
                         << 8));

                RelocationRange range;

                range.sourceBegin =
                    source;

                range.destinationBegin =
                    destination;

                range.size =
                    256;

                const auto duplicate =
                    std::find_if(
                        result.ranges.begin(),
                        result.ranges.end(),
                        [&](const RelocationRange&
                                existing)
                        {
                            return
                                existing.sourceBegin ==
                                    range.sourceBegin &&
                                existing.destinationBegin ==
                                    range.destinationBegin &&
                                existing.size ==
                                    range.size;
                        });

                if (duplicate ==
                    result.ranges.end())
                {
                    result.ranges.push_back(
                        range);
                }

                //
                // Пропускаем найденную пару
                // LDA/STA.
                //
                candidate += 5;
            }
        }

        return result;
    }
};

} // namespace atari