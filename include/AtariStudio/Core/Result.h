#pragma once

#include <string>
#include <utility>

namespace atari
{

    class Result
    {
    public:

        Result() = default;

        static Result Success()
        {
            return Result(true, {});
        }

        static Result Failure(std::string message)
        {
            return Result(false, std::move(message));
        }

        [[nodiscard]]
        bool Succeeded() const noexcept
        {
            return m_success;
        }

        [[nodiscard]]
        bool Failed() const noexcept
        {
            return !m_success;
        }

        [[nodiscard]]
        const std::string& Message() const noexcept
        {
            return m_message;
        }

        explicit operator bool() const noexcept
        {
            return m_success;
        }

    private:

        Result(bool success, std::string message)
            : m_success(success)
            , m_message(std::move(message))
        {
        }

    private:

        bool m_success = true;
        std::string m_message;
    };

}
