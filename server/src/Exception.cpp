#include <RTypeSrv/Exception.hpp>

namespace rtype::srv {

/**
 * @brief Returns the exception message.
 * @return The exception message.
 */
const char *Exception::what() const noexcept
{
    return _what.c_str();
}

/**
 * @brief Returns the location where the exception was thrown.
 * @return The location where the exception was thrown.
 */
const char *Exception::where() const noexcept
{
    return _where.c_str();
}

}// namespace rtype::srv
