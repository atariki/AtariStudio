#pragma once

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <AtariStudio/Core/Project.h>
#include <AtariStudio/Disassembler/StructuredCodeGenerator.h>

namespace atari
{

class StructuredTranslationUnitGenerator
{
public:

    [[nodiscard]]
    std::string Generate(
        const StructuredExpressionResult& result) const
    {
        return GenerateImpl(
            result,
            nullptr);
    }

    [[nodiscard]]
    std::string Generate(
        const Project& project,
        const StructuredExpressionResult& result) const
    {
        return GenerateImpl(
            result,
            &project);
    }

private:

    [[nodiscard]]
    static std::string GenerateImpl(
        const StructuredExpressionResult& result,
        const Project* project)
    {
        const std::string body =
            Normalize(
                StructuredCodeGenerator{}
                    .Generate(result));

        const auto routines =
            CollectRoutineNames(
                result);

        const auto externalRoutines =
            CollectExternalRoutines(
                body,
                routines);

        std::string output =
            RuntimePrelude();

        output +=
            "\n// Recovered routine declarations.\n";

        for (const auto& routine : routines)
        {
            output +=
                "void " +
                routine +
                "();\n";
        }

        if (!externalRoutines.empty())
        {
            output +=
                "\n// Calls outside the recovered image are routed through a hook.\n";

            for (const auto& routine :
                 externalRoutines)
            {
                output +=
                    "inline void " +
                    routine +
                    "() { call_external6502(\"" +
                    routine +
                    "\"); }\n";
            }
        }

        output +=
            GenerateImageInitializer(
                project);

        output +=
            GenerateEntryWrapper(
                project,
                result);

        output += '\n';
        output += body;

        return output;
    }

    [[nodiscard]]
    static std::string RuntimePrelude()
    {
        return R"cpp(#include <array>
#include <cstddef>
#include <cstdint>

using byte6502 = std::uint8_t;
using word6502 = std::uint16_t;

std::array<byte6502, 65536> memory{};

byte6502 A = 0;
byte6502 X = 0;
byte6502 Y = 0;
byte6502 SP = 0xFF;
byte6502 P = 0;

bool C = false;
bool D = false;
bool I = false;
bool V = false;
bool N = false;
bool Z = false;

[[nodiscard]] constexpr byte6502 byte(
    std::uint32_t value) noexcept
{
    return static_cast<byte6502>(value);
}

[[nodiscard]] inline word6502 word(
    word6502 address) noexcept
{
    const word6502 next =
        static_cast<word6502>(address + 1);

    return static_cast<word6502>(
        memory[address] |
        (static_cast<word6502>(
            memory[next]) << 8));
}

struct word_reference6502
{
    word6502 address = 0;

    [[nodiscard]] operator word6502() const noexcept
    {
        return word(address);
    }

    word_reference6502& operator=(
        word6502 value) noexcept
    {
        memory[address] =
            byte(value);

        memory[static_cast<word6502>(
            address + 1)] =
                byte(value >> 8);

        return *this;
    }

    word_reference6502& operator+=(
        word6502 value) noexcept
    {
        return
            *this =
                static_cast<word6502>(
                    word(address) + value);
    }

    word_reference6502& operator-=(
        word6502 value) noexcept
    {
        return
            *this =
                static_cast<word6502>(
                    word(address) - value);
    }
};

struct word_memory6502
{
    std::array<
        word_reference6502,
        65536> references{};

    word_memory6502() noexcept
    {
        for (std::size_t index = 0;
             index < references.size();
             ++index)
        {
            references[index].address =
                static_cast<word6502>(
                    index);
        }
    }

    [[nodiscard]] word_reference6502& operator[](
        std::size_t address) noexcept
    {
        return references[
            static_cast<word6502>(
                address)];
    }
};

word_memory6502 memory16{};

inline void set_nz6502(
    byte6502 value,
    bool* negative,
    bool* zero) noexcept
{
    *negative = (value & 0x80) != 0;
    *zero = value == 0;
}

template<typename Destination>
inline void load6502(
    byte6502 source,
    Destination* destination,
    bool* negative,
    bool* zero) noexcept
{
    *destination = source;
    set_nz6502(source, negative, zero);
}

template<typename Destination>
inline void transfer6502(
    byte6502 source,
    Destination* destination,
    bool* negative,
    bool* zero) noexcept
{
    load6502(
        source,
        destination,
        negative,
        zero);
}

template<typename Destination>
inline void inc6502_n(
    Destination* destination,
    std::size_t count,
    bool* negative,
    bool* zero) noexcept
{
    byte6502 result =
        static_cast<byte6502>(
            static_cast<std::uint32_t>(
                static_cast<byte6502>(
                    *destination)) +
            count);

    *destination = result;
    set_nz6502(result, negative, zero);
}

template<typename Destination>
inline void dec6502_n(
    Destination* destination,
    std::size_t count,
    bool* negative,
    bool* zero) noexcept
{
    byte6502 result =
        static_cast<byte6502>(
            static_cast<std::uint32_t>(
                static_cast<byte6502>(
                    *destination)) -
            count);

    *destination = result;
    set_nz6502(result, negative, zero);
}

template<typename Destination, typename Register>
inline void inc_load6502(
    Destination* destination,
    std::size_t count,
    Register* target,
    bool* negative,
    bool* zero) noexcept
{
    inc6502_n(
        destination,
        count,
        negative,
        zero);

    *target =
        static_cast<byte6502>(
            *destination);
}

template<typename Destination, typename Register>
inline void dec_load6502(
    Destination* destination,
    std::size_t count,
    Register* target,
    bool* negative,
    bool* zero) noexcept
{
    dec6502_n(
        destination,
        count,
        negative,
        zero);

    *target =
        static_cast<byte6502>(
            *destination);
}

inline void inc16_6502(
    word_reference6502* destination,
    bool* negative,
    bool* zero) noexcept
{
    const word6502 result =
        static_cast<word6502>(
            static_cast<word6502>(
                *destination) + 1);

    *destination = result;
    set_nz6502(
        byte(result >> 8),
        negative,
        zero);
}

inline void dec16_6502(
    word_reference6502* destination,
    byte6502* accumulator,
    bool* negative,
    bool* zero) noexcept
{
    const word6502 result =
        static_cast<word6502>(
            static_cast<word6502>(
                *destination) - 1);

    *destination = result;
    *accumulator = byte(result);
    set_nz6502(
        *accumulator,
        negative,
        zero);
}

inline void compare6502(
    byte6502 source,
    byte6502 operand,
    bool* carry,
    bool* negative,
    bool* zero) noexcept
{
    const byte6502 result =
        byte(source - operand);

    *carry = source >= operand;
    set_nz6502(
        result,
        negative,
        zero);
}

inline void compare(
    byte6502,
    byte6502) noexcept
{
}

inline void test_bits6502(
    byte6502 accumulator,
    byte6502 operand,
    bool* overflow,
    bool* negative,
    bool* zero) noexcept
{
    *overflow = (operand & 0x40) != 0;
    *negative = (operand & 0x80) != 0;
    *zero = (accumulator & operand) == 0;
}

inline void test_bits(
    byte6502,
    byte6502) noexcept
{
}

[[nodiscard]] inline byte6502 adc6502_value(
    byte6502 source,
    byte6502 operand,
    bool carry,
    bool decimal) noexcept
{
    std::uint16_t result =
        static_cast<std::uint16_t>(
            source) +
        operand +
        (carry ? 1 : 0);

    if (decimal)
    {
        if (((source & 0x0F) +
             (operand & 0x0F) +
             (carry ? 1 : 0)) > 9)
        {
            result += 0x06;
        }

        if (result > 0x99)
        {
            result += 0x60;
        }
    }

    return byte(result);
}

[[nodiscard]] inline byte6502 sbc6502_value(
    byte6502 source,
    byte6502 operand,
    bool carry,
    bool decimal) noexcept
{
    std::int16_t result =
        static_cast<std::int16_t>(
            source) -
        operand -
        (carry ? 0 : 1);

    if (decimal)
    {
        if (static_cast<std::int16_t>(
                source & 0x0F) -
                (carry ? 0 : 1) <
            static_cast<std::int16_t>(
                operand & 0x0F))
        {
            result -= 0x06;
        }

        if (result < 0)
        {
            result -= 0x60;
        }
    }

    return byte(
        static_cast<std::uint16_t>(
            result));
}

inline void adc6502(
    byte6502 source,
    byte6502 operand,
    bool carryInput,
    bool decimal,
    byte6502* destination,
    bool* carry,
    bool* overflow,
    bool* negative,
    bool* zero) noexcept
{
    const std::uint16_t binary =
        static_cast<std::uint16_t>(
            source) +
        operand +
        (carryInput ? 1 : 0);

    *destination =
        adc6502_value(
            source,
            operand,
            carryInput,
            decimal);

    *carry = decimal
        ? (static_cast<unsigned>(
               source / 16) * 10 +
           source % 16 +
           static_cast<unsigned>(
               operand / 16) * 10 +
           operand % 16 +
           (carryInput ? 1 : 0)) > 99
        : binary > 0xFF;

    *overflow =
        ((~(source ^ operand) &
          (source ^ byte(binary)) &
          0x80) != 0);

    set_nz6502(
        byte(binary),
        negative,
        zero);
}

inline void sbc6502(
    byte6502 source,
    byte6502 operand,
    bool carryInput,
    bool decimal,
    byte6502* destination,
    bool* carry,
    bool* overflow,
    bool* negative,
    bool* zero) noexcept
{
    const std::int16_t binary =
        static_cast<std::int16_t>(
            source) -
        operand -
        (carryInput ? 0 : 1);

    *destination =
        sbc6502_value(
            source,
            operand,
            carryInput,
            decimal);

    *carry = binary >= 0;
    *overflow =
        (((source ^ operand) &
          (source ^ byte(
              static_cast<std::uint16_t>(
                  binary))) &
          0x80) != 0);

    set_nz6502(
        byte(
            static_cast<std::uint16_t>(
                binary)),
        negative,
        zero);
}

inline void adc16_6502(
    word6502 source,
    word6502 operand,
    bool carryInput,
    bool decimal,
    word_reference6502* destination,
    byte6502* accumulator,
    bool* carry,
    bool* overflow,
    bool* negative,
    bool* zero) noexcept
{
    if (decimal)
    {
        byte6502 low = 0;
        byte6502 high = 0;
        bool lowCarry = false;
        bool ignoredOverflow = false;
        bool ignoredNegative = false;
        bool ignoredZero = false;

        adc6502(
            byte(source),
            byte(operand),
            carryInput,
            true,
            &low,
            &lowCarry,
            &ignoredOverflow,
            &ignoredNegative,
            &ignoredZero);

        adc6502(
            byte(source >> 8),
            byte(operand >> 8),
            lowCarry,
            true,
            &high,
            carry,
            overflow,
            negative,
            zero);

        *destination =
            static_cast<word6502>(
                low |
                (static_cast<word6502>(
                    high) << 8));

        *accumulator = high;
        return;
    }

    const std::uint32_t wide =
        static_cast<std::uint32_t>(
            source) +
        operand +
        (carryInput ? 1 : 0);

    const word6502 result =
        static_cast<word6502>(
            wide);

    *destination = result;
    *accumulator = byte(result >> 8);
    *carry = wide > 0xFFFF;
    *overflow =
        ((~(source ^ operand) &
          (source ^ result) &
          0x8000) != 0);

    set_nz6502(
        *accumulator,
        negative,
        zero);
}

inline void sbc16_6502(
    word6502 source,
    word6502 operand,
    bool carryInput,
    bool decimal,
    word_reference6502* destination,
    byte6502* accumulator,
    bool* carry,
    bool* overflow,
    bool* negative,
    bool* zero) noexcept
{
    if (decimal)
    {
        byte6502 low = 0;
        byte6502 high = 0;
        bool lowCarry = false;
        bool ignoredOverflow = false;
        bool ignoredNegative = false;
        bool ignoredZero = false;

        sbc6502(
            byte(source),
            byte(operand),
            carryInput,
            true,
            &low,
            &lowCarry,
            &ignoredOverflow,
            &ignoredNegative,
            &ignoredZero);

        sbc6502(
            byte(source >> 8),
            byte(operand >> 8),
            lowCarry,
            true,
            &high,
            carry,
            overflow,
            negative,
            zero);

        *destination =
            static_cast<word6502>(
                low |
                (static_cast<word6502>(
                    high) << 8));

        *accumulator = high;
        return;
    }

    const std::int32_t wide =
        static_cast<std::int32_t>(
            source) -
        operand -
        (carryInput ? 0 : 1);

    const word6502 result =
        static_cast<word6502>(
            wide);

    *destination = result;
    *accumulator = byte(result >> 8);
    *carry = wide >= 0;
    *overflow =
        (((source ^ operand) &
          (source ^ result) &
          0x8000) != 0);

    set_nz6502(
        *accumulator,
        negative,
        zero);
}

[[nodiscard]] inline byte6502 asl6502_value(
    byte6502 source) noexcept
{
    return byte(
        static_cast<std::uint16_t>(
            source) << 1);
}

[[nodiscard]] inline byte6502 lsr6502_value(
    byte6502 source) noexcept
{
    return byte(source >> 1);
}

[[nodiscard]] inline byte6502 rol6502_value(
    byte6502 source,
    bool carry) noexcept
{
    return byte(
        (static_cast<std::uint16_t>(
             source) << 1) |
        (carry ? 1 : 0));
}

[[nodiscard]] inline byte6502 ror6502_value(
    byte6502 source,
    bool carry) noexcept
{
    return byte(
        (source >> 1) |
        (carry ? 0x80 : 0));
}

template<typename Destination>
inline void asl6502(
    byte6502 source,
    Destination* destination,
    bool* carry,
    bool* negative,
    bool* zero) noexcept
{
    *carry = (source & 0x80) != 0;
    const byte6502 result =
        asl6502_value(source);
    *destination = result;
    set_nz6502(result, negative, zero);
}

template<typename Destination>
inline void lsr6502(
    byte6502 source,
    Destination* destination,
    bool* carry,
    bool* negative,
    bool* zero) noexcept
{
    *carry = (source & 0x01) != 0;
    const byte6502 result =
        lsr6502_value(source);
    *destination = result;
    set_nz6502(result, negative, zero);
}

template<typename Destination>
inline void rol6502(
    byte6502 source,
    bool carryInput,
    Destination* destination,
    bool* carry,
    bool* negative,
    bool* zero) noexcept
{
    *carry = (source & 0x80) != 0;
    const byte6502 result =
        rol6502_value(
            source,
            carryInput);
    *destination = result;
    set_nz6502(result, negative, zero);
}

template<typename Destination>
inline void ror6502(
    byte6502 source,
    bool carryInput,
    Destination* destination,
    bool* carry,
    bool* negative,
    bool* zero) noexcept
{
    *carry = (source & 0x01) != 0;
    const byte6502 result =
        ror6502_value(
            source,
            carryInput);
    *destination = result;
    set_nz6502(result, negative, zero);
}

inline void push(
    byte6502 value) noexcept
{
    memory[
        static_cast<word6502>(
            0x0100 | SP)] =
                value;
    --SP;
}

[[nodiscard]] inline byte6502 pop() noexcept
{
    ++SP;
    return memory[
        static_cast<word6502>(
            0x0100 | SP)];
}

[[nodiscard]] inline byte6502 status6502(
    bool breakFlag = false) noexcept
{
    P =
        static_cast<byte6502>(
            0x20 |
            (C ? 0x01 : 0) |
            (Z ? 0x02 : 0) |
            (I ? 0x04 : 0) |
            (D ? 0x08 : 0) |
            (breakFlag ? 0x10 : 0) |
            (V ? 0x40 : 0) |
            (N ? 0x80 : 0));

    return P;
}

inline void set_status6502(
    byte6502 value) noexcept
{
    P = value;
    C = (value & 0x01) != 0;
    Z = (value & 0x02) != 0;
    I = (value & 0x04) != 0;
    D = (value & 0x08) != 0;
    V = (value & 0x40) != 0;
    N = (value & 0x80) != 0;
}

inline void push_status6502() noexcept
{
    push(
        status6502(true));
}

inline void pull_status6502() noexcept
{
    set_status6502(
        pop());
}

using interrupt_hook6502 =
    void (*)(
        word6502 target,
        bool returning);

interrupt_hook6502 interrupt6502 = nullptr;

inline void dispatch_interrupt6502(
    word6502 target,
    bool returning)
{
    if (interrupt6502 != nullptr)
    {
        interrupt6502(
            target,
            returning);
    }
}

inline void brk6502(
    word6502 programCounter)
{
    const word6502 returnAddress =
        static_cast<word6502>(
            programCounter + 2);

    push(
        byte(returnAddress >> 8));

    push(
        byte(returnAddress));

    push_status6502();
    I = true;

    dispatch_interrupt6502(
        word(0xFFFE),
        false);
}

[[nodiscard]] inline word6502 rti6502()
{
    pull_status6502();

    const byte6502 low =
        pop();

    const byte6502 high =
        pop();

    const word6502 programCounter =
        static_cast<word6502>(
            low |
            (static_cast<word6502>(
                high) << 8));

    dispatch_interrupt6502(
        programCounter,
        true);

    return programCounter;
}

inline void pull6502(
    byte6502* destination,
    bool* negative,
    bool* zero) noexcept
{
    load6502(
        pop(),
        destination,
        negative,
        zero);
}

using external_call6502 =
    void (*)(const char* routine);

external_call6502 external6502 = nullptr;

using unresolved_flow_hook6502 =
    bool (*)();

unresolved_flow_hook6502
    unresolvedFlow6502 = nullptr;

inline void call_external6502(
    const char* routine)
{
    if (external6502 != nullptr)
    {
        external6502(routine);
    }
}

[[nodiscard]] inline bool
    unresolved_flow6502()
{
    return unresolvedFlow6502 != nullptr
        ? unresolvedFlow6502()
        : true;
}
)cpp";
    }

    [[nodiscard]]
    static std::string GenerateImageInitializer(
        const Project* project)
    {
        std::string output =
            "\nvoid initialize_image6502()\n"
            "{\n"
            "    memory.fill(0);\n"
            "    A = 0;\n"
            "    X = 0;\n"
            "    Y = 0;\n"
            "    SP = 0xFF;\n"
            "    P = 0;\n"
            "    C = false;\n"
            "    D = false;\n"
            "    I = false;\n"
            "    V = false;\n"
            "    N = false;\n"
            "    Z = false;\n";

        if (project != nullptr)
        {
            const auto& memoryImage =
                project->GetMemory();

            std::uint32_t address = 0;

            while (address <= 0xFFFF)
            {
                if (!memoryImage.Cell(
                        static_cast<u16>(
                            address)).initialized)
                {
                    ++address;
                    continue;
                }

                const std::uint32_t begin =
                    address;

                while (address < 0xFFFF &&
                       memoryImage.Cell(
                           static_cast<u16>(
                               address + 1)).
                           initialized)
                {
                    ++address;
                }

                const std::uint32_t end =
                    address;

                output +=
                    "\n    static constexpr byte6502 image_" +
                    Hex(
                        static_cast<u16>(
                            begin),
                        4) +
                    "[] =\n"
                    "    {\n        ";

                std::size_t column = 0;

                for (std::uint32_t current =
                         begin;
                     current <= end;
                     ++current)
                {
                    if (column == 12)
                    {
                        output +=
                            "\n        ";
                        column = 0;
                    }

                    output +=
                        "0x" +
                        Hex(
                            memoryImage.Read8(
                                static_cast<u16>(
                                    current)),
                            2);

                    if (current != end)
                    {
                        output += ", ";
                    }

                    ++column;
                }

                const std::string imageName =
                    "image_" +
                    Hex(
                        static_cast<u16>(
                            begin),
                        4);

                output +=
                    "\n    };\n"
                    "\n"
                    "    for (std::size_t index = 0;\n"
                    "         index < sizeof(" +
                    imageName +
                    ");\n"
                    "         ++index)\n"
                    "    {\n"
                    "        memory[0x" +
                    Hex(
                        static_cast<u16>(
                            begin),
                        4) +
                    " + index] =\n"
                    "            " +
                    imageName +
                    "[index];\n"
                    "    }\n";

                ++address;
            }
        }

        output += "}\n";
        return output;
    }

    [[nodiscard]]
    static std::string GenerateEntryWrapper(
        const Project* project,
        const StructuredExpressionResult& result)
    {
        std::string output =
            "\nvoid run_entry6502()\n"
            "{\n";

        if (project != nullptr)
        {
            u16 entry =
                project->RunAddress();

            if (entry == 0)
            {
                entry =
                    project->InitAddress();
            }

            for (const auto& root :
                 result.roots)
            {
                if (root.kind ==
                        StructuredExpressionKind::Block &&
                    root.address == entry)
                {
                    output +=
                        "    " +
                        Identifier(
                            root.statement,
                            root.address) +
                        "();\n";

                    break;
                }
            }
        }

        output += "}\n";
        return output;
    }

    [[nodiscard]]
    static std::string Hex(
        u16 value,
        int width)
    {
        std::ostringstream stream;

        stream
            << std::uppercase
            << std::hex
            << std::setw(width)
            << std::setfill('0')
            << value;

        return stream.str();
    }

    [[nodiscard]]
    static std::string Normalize(
        const std::string& source)
    {
        std::string result;
        result.reserve(
            source.size() + 32);

        for (std::size_t index = 0;
             index < source.size();
             ++index)
        {
            if (source[index] == '$' &&
                index + 1 < source.size() &&
                std::isxdigit(
                    static_cast<unsigned char>(
                        source[index + 1])))
            {
                result += "0x";
                continue;
            }

            result += source[index];
        }

        ReplaceAll(
            result,
            "for (;;)",
            "while (unresolved_flow6502())");

        ReplaceAll(
            result,
            "rti6502();",
            "static_cast<void>(rti6502());");

        ReplaceAll(
            result,
            "return;",
            "if (unresolved_flow6502()) return;");

        ReplaceAll(
            result,
            "return /* RTI */;",
            "if (unresolved_flow6502()) return /* RTI */;");

        ReplaceAll(
            result,
            "return /* BRK */;",
            "if (unresolved_flow6502()) return /* BRK */;");

        ReplaceAll(
            result,
            "push(P)",
            "push_status6502()");

        ReplaceAll(
            result,
            "P = pop()",
            "pull_status6502()");

        return CommentAssemblyStatements(
            result);
    }

    static void ReplaceAll(
        std::string& text,
        std::string_view from,
        std::string_view to)
    {
        std::size_t position = 0;

        while ((position =
                    text.find(
                        from,
                        position)) !=
               std::string::npos)
        {
            text.replace(
                position,
                from.size(),
                to);

            position += to.size();
        }
    }

    [[nodiscard]]
    static std::string CommentAssemblyStatements(
        const std::string& source)
    {
        static constexpr std::string_view
            Mnemonics[] =
            {
                "BCC", "BCS", "BEQ", "BMI",
                "BNE", "BPL", "BRK", "BVC",
                "BVS", "JMP", "RTI"
            };

        std::string output;
        std::size_t begin = 0;

        while (begin < source.size())
        {
            const std::size_t end =
                source.find(
                    '\n',
                    begin);

            const std::size_t length =
                end == std::string::npos
                    ? source.size() - begin
                    : end - begin;

            const std::string_view line(
                source.data() + begin,
                length);

            const std::size_t first =
                line.find_first_not_of(
                    ' ');

            bool assembly = false;

            if (first !=
                std::string_view::npos)
            {
                for (const auto mnemonic :
                     Mnemonics)
                {
                    if (line.substr(
                            first,
                            mnemonic.size()) ==
                            mnemonic &&
                        (first + mnemonic.size() ==
                             line.size() ||
                         std::isspace(
                             static_cast<unsigned char>(
                                 line[first +
                                      mnemonic.size()])) ||
                         line[first +
                              mnemonic.size()] == ';'))
                    {
                        assembly = true;
                        break;
                    }
                }
            }

            if (assembly)
            {
                output.append(
                    line.substr(
                        0,
                        first));
                output += "/* 6502: ";
                output.append(
                    line.substr(first));
                output += " */";
            }
            else
            {
                output.append(line);
            }

            if (end !=
                std::string::npos)
            {
                output += '\n';
                begin = end + 1;
            }
            else
            {
                break;
            }
        }

        return output;
    }

    [[nodiscard]]
    static std::vector<std::string>
        CollectRoutineNames(
            const StructuredExpressionResult& result)
    {
        std::vector<std::string> names;

        for (const auto& root : result.roots)
        {
            if (root.kind !=
                StructuredExpressionKind::Block)
            {
                continue;
            }

            names.push_back(
                Identifier(
                    root.statement,
                    root.address));
        }

        SortUnique(names);
        return names;
    }

    [[nodiscard]]
    static std::vector<std::string>
        CollectExternalRoutines(
            const std::string& body,
            const std::vector<std::string>& routines)
    {
        std::vector<std::string> calls;

        for (std::size_t index = 0;
             index < body.size();)
        {
            if (!IsIdentifierStart(
                    body[index]))
            {
                ++index;
                continue;
            }

            const std::size_t begin =
                index++;

            while (index < body.size() &&
                   IsIdentifierPart(
                       body[index]))
            {
                ++index;
            }

            std::size_t parenthesis =
                index;

            while (parenthesis <
                       body.size() &&
                   std::isspace(
                       static_cast<unsigned char>(
                           body[parenthesis])))
            {
                ++parenthesis;
            }

            if (parenthesis >=
                    body.size() ||
                body[parenthesis] != '(')
            {
                continue;
            }

            const std::string name =
                body.substr(
                    begin,
                    index - begin);

            if (!IsKnownCallable(name) &&
                !std::binary_search(
                    routines.begin(),
                    routines.end(),
                    name))
            {
                calls.push_back(name);
            }
        }

        SortUnique(calls);
        return calls;
    }

    [[nodiscard]]
    static bool IsKnownCallable(
        const std::string& name)
    {
        static constexpr std::string_view
            Names[] =
            {
                "adc16_6502", "adc6502",
                "adc6502_value", "asl6502",
                "asl6502_value", "brk6502",
                "byte",
                "compare", "compare6502",
                "dec16_6502", "dec6502_n",
                "dec_load6502", "for", "if",
                "inc16_6502", "inc6502_n",
                "inc_load6502", "load6502",
                "lsr6502", "lsr6502_value",
                "pop", "pull6502", "push",
                "pull_status6502",
                "push_status6502",
                "rol6502", "rol6502_value",
                "ror6502", "ror6502_value",
                "rti6502",
                "sbc16_6502", "sbc6502",
                "sbc6502_value", "test_bits",
                "test_bits6502", "transfer6502",
                "unresolved_flow6502",
                "while", "word"
            };

        return std::find(
                   std::begin(Names),
                   std::end(Names),
                   name) !=
               std::end(Names);
    }

    [[nodiscard]]
    static std::string Identifier(
        std::string value,
        u16 address)
    {
        for (char& character : value)
        {
            if (!IsIdentifierPart(
                    character))
            {
                character = '_';
            }
        }

        if (value.empty())
        {
            constexpr char Digits[] =
                "0123456789ABCDEF";

            value = "routine_0000";

            for (int index = 0;
                 index < 4;
                 ++index)
            {
                value[
                    value.size() - 1 -
                    index] =
                        Digits[
                            (address >>
                             (index * 4)) &
                            0x0F];
            }
        }

        if (!IsIdentifierStart(
                value.front()) ||
            IsCppKeyword(value))
        {
            value =
                "routine_" +
                value;
        }

        return value;
    }

    [[nodiscard]]
    static bool IsCppKeyword(
        const std::string& value) noexcept
    {
        static constexpr std::string_view
            Keywords[] =
            {
                "asm", "auto", "bool", "break",
                "case", "catch", "char",
                "class", "const", "continue",
                "default", "delete", "do",
                "double", "else", "enum",
                "explicit", "export", "extern",
                "false", "float", "for",
                "friend", "goto", "if",
                "inline", "int", "long",
                "namespace", "new", "operator",
                "private", "protected",
                "public", "register", "return",
                "short", "signed", "sizeof",
                "static", "struct", "switch",
                "template", "this", "throw",
                "true", "try", "typedef",
                "typename", "union", "unsigned",
                "using", "virtual", "void",
                "volatile", "while"
            };

        return std::find(
                   std::begin(Keywords),
                   std::end(Keywords),
                   value) !=
               std::end(Keywords);
    }

    [[nodiscard]]
    static bool IsIdentifierStart(
        char character) noexcept
    {
        return
            character == '_' ||
            std::isalpha(
                static_cast<unsigned char>(
                    character)) != 0;
    }

    [[nodiscard]]
    static bool IsIdentifierPart(
        char character) noexcept
    {
        return
            character == '_' ||
            std::isalnum(
                static_cast<unsigned char>(
                    character)) != 0;
    }

    static void SortUnique(
        std::vector<std::string>& values)
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
