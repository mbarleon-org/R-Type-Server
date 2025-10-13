#pragma once

#include <RTypeSrv/Api.hpp>
#include <exception>
#include <sstream>
#include <string>
#include <utility>

namespace rtype::srv {

#ifdef _WIN32
    #pragma warning(push)
    #pragma warning(disable : 4275)
    #pragma warning(disable : 4251)
#endif

/**
 * @brief A custom exception class for the R-Type server.
 */
class RTYPE_SRV_API Exception final : public std::exception
{
    public:
        /**
         * @brief Constructs a new Exception object.
         * @param where The location where the exception occurred.
         * @param args The arguments to concatenate into the error message.
         */
        template<typename... Args>
        constexpr explicit Exception(std::string where, Args &&...args)
            : _where(std::move(where)), _what(concatStrings(std::forward<Args>(args)...))
        {
        }
        ~Exception() noexcept override = default;

        /**
         * @brief Gets the error message.
         * @return The error message.
         */
        [[nodiscard]] const char *what() const noexcept override;

        /**
         * @brief Gets the location where the exception occurred.
         * @return The location where the exception occurred.
         */
        [[nodiscard]] const char *where() const noexcept;

    private:
        const std::string _where;
        const std::string _what;

        /**
         * @brief Concatenates a series of arguments into a single string.
         * @param args The arguments to concatenate.
         * @return The concatenated string.
         */
        template<typename... Args>
        static std::string concatStrings(Args &&...args)
        {
            std::ostringstream oss;
            if constexpr (sizeof...(args) > 0) {
                (oss << ... << args);
            }
            return oss.str();
        }
};

#ifdef _WIN32
    #pragma warning(pop)
#endif

}// namespace rtype::srv
