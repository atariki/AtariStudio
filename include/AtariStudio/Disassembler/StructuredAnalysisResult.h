#pragma once

#include <string>

#include <AtariStudio/Disassembler/StructuredExpressionBuilder.h>


namespace atari
{

struct StructuredAnalysisResult
{
    StructuredExpressionResult expressions;

    std::string generatedCode;

    std::string generatedTranslationUnit;

    [[nodiscard]]
    std::size_t ExpressionCount() const noexcept
    {
        return
            expressions.ExpressionCount();
    }

    [[nodiscard]]
    std::size_t StatementCount() const noexcept
    {
        return
            expressions.StatementCount();
    }

    [[nodiscard]]
    const std::string& GeneratedCode() const noexcept
    {
        return
            generatedCode;
    }

    [[nodiscard]]
    const std::string&
        GeneratedTranslationUnit() const noexcept
    {
        return
            generatedTranslationUnit;
    }
};


} // namespace atari
