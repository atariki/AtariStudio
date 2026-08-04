#pragma once

#include <cctype>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <AtariStudio/Core/Types.h>
#include <AtariStudio/Disassembler/StructuredExpressionBuilder.h>

namespace atari
{

class StructuredCodeGenerator
{
public:

    [[nodiscard]]
    std::string Generate(
        const StructuredExpressionResult& result) const
    {
        std::string output;

        for (std::size_t index = 0;
             index < result.roots.size();
             ++index)
        {
            if (index != 0)
            {
                output += '\n';
            }

            GenerateRoot(
                result.roots[index],
                output);
        }

        return output;
    }

private:

    static void GenerateRoot(
        const StructuredExpression& root,
        std::string& output)
    {
        if (root.kind !=
            StructuredExpressionKind::Block)
        {
            GenerateNode(
                root,
                output,
                0);

            return;
        }

        output +=
            "void ";

        output +=
            RoutineIdentifier(
                root);

        output +=
            "()\n{\n";

        for (const auto& child :
             root.children)
        {
            GenerateNode(
                child,
                output,
                1);
        }

        output +=
            "}\n";
    }

    static void GenerateNode(
        const StructuredExpression& node,
        std::string& output,
        std::size_t depth)
    {
        switch (node.kind)
        {
        case StructuredExpressionKind::Block:

            for (const auto& child :
                 node.children)
            {
                GenerateNode(
                    child,
                    output,
                    depth);
            }

            break;

        case StructuredExpressionKind::If:

            WriteConditionalHeader(
                "if",
                node.condition,
                output,
                depth);

            GenerateBody(
                node.children,
                output,
                depth);

            break;

        case StructuredExpressionKind::IfElse:

            WriteConditionalHeader(
                "if",
                node.condition,
                output,
                depth);

            GenerateBody(
                node.children,
                output,
                depth);

            WriteIndent(
                output,
                depth);

            output +=
                "else\n";

            GenerateBody(
                node.elseChildren,
                output,
                depth);

            break;

        case StructuredExpressionKind::While:

            WriteConditionalHeader(
                "while",
                node.condition,
                output,
                depth);

            GenerateBody(
                node.children,
                output,
                depth);

            break;

        case StructuredExpressionKind::DoWhile:

            WriteIndent(
                output,
                depth);

            output +=
                "do\n";

            GenerateBody(
                node.children,
                output,
                depth);

            WriteIndent(
                output,
                depth);

            output +=
                "while (";

            output +=
                SafeCondition(
                    node.condition);

            output +=
                ");\n";

            break;

        case StructuredExpressionKind::InfiniteLoop:

            WriteIndent(
                output,
                depth);

            output +=
                "for (;;)\n";

            GenerateBody(
                node.children,
                output,
                depth);

            break;

        case StructuredExpressionKind::Break:

            WriteIndent(
                output,
                depth);

            output +=
                "break;\n";

            break;

        case StructuredExpressionKind::Continue:

            WriteIndent(
                output,
                depth);

            output +=
                "continue;\n";

            break;

        case StructuredExpressionKind::Statement:

            WriteIndent(
                output,
                depth);

            output +=
                node.statement;

            if (NeedsTerminator(
                    node.statement))
            {
                output +=
                    ';';
            }

            output +=
                '\n';

            break;

        case StructuredExpressionKind::Empty:
        default:

            break;
        }
    }

    static void WriteConditionalHeader(
        const char* keyword,
        const std::string& condition,
        std::string& output,
        std::size_t depth)
    {
        WriteIndent(
            output,
            depth);

        output +=
            keyword;

        output +=
            " (";

        output +=
            SafeCondition(
                condition);

        output +=
            ")\n";
    }

    static void GenerateBody(
        const std::vector<StructuredExpression>& children,
        std::string& output,
        std::size_t depth)
    {
        WriteIndent(
            output,
            depth);

        output +=
            "{\n";

        for (const auto& child :
             children)
        {
            GenerateNode(
                child,
                output,
                depth + 1);
        }

        WriteIndent(
            output,
            depth);

        output +=
            "}\n";
    }

    [[nodiscard]]
    static std::string RoutineIdentifier(
        const StructuredExpression& root)
    {
        std::string identifier =
            root.statement;

        if (identifier.empty())
        {
            return
                "routine_" +
                AddressText(
                    root.address);
        }

        for (char& character :
             identifier)
        {
            const auto value =
                static_cast<unsigned char>(
                    character);

            if (!std::isalnum(value) &&
                character != '_')
            {
                character =
                    '_';
            }
        }

        if (identifier.empty() ||
            std::isdigit(
                static_cast<unsigned char>(
                    identifier.front())) ||
            IsCppKeyword(identifier))
        {
            identifier =
                "routine_" +
                identifier;
        }

        return identifier;
    }

    [[nodiscard]]
    static bool IsCppKeyword(
        const std::string& value) noexcept
    {
        static constexpr const char*
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

        for (const char* keyword :
             Keywords)
        {
            if (value == keyword)
            {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]]
    static std::string AddressText(
        u16 address)
    {
        std::ostringstream stream;

        stream
            << std::uppercase
            << std::hex
            << std::setw(4)
            << std::setfill('0')
            << address;

        return
            stream.str();
    }

    [[nodiscard]]
    static const std::string& SafeCondition(
        const std::string& condition)
    {
        static const std::string UnknownCondition =
            "/* unresolved */ true";

        if (condition.empty())
        {
            return
                UnknownCondition;
        }

        return
            condition;
    }

    [[nodiscard]]
    static bool NeedsTerminator(
        const std::string& statement) noexcept
    {
        if (statement.empty())
        {
            return false;
        }

        const char last =
            statement.back();

        return
            last != ';' &&
            last != '{' &&
            last != '}';
    }

    static void WriteIndent(
        std::string& output,
        std::size_t depth)
    {
        output.append(
            depth * 4,
            ' ');
    }
};

} // namespace atari
