#pragma once

#include <RTypeSrv/Api.hpp>
#include <string>

#define RTYPE_SRV_VER "0.0.1"

namespace rtype::srv {

/**
 * @brief Returns the version of the R-Type server library.
 * @return The version of the R-Type server library.
 */
[[nodiscard]] RTYPE_SRV_API const char *version() noexcept;

/**
 * @brief Returns the full version of the R-Type server library.
 * @return The full version of the R-Type server library.
 */
[[nodiscard]] RTYPE_SRV_API std::string full_version() noexcept;

}// namespace rtype::srv
