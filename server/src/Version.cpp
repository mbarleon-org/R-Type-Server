#include <RTypeSrv/Version.hpp>

/**
 * @brief Returns the version of the R-Type server library.
 * @return The version of the R-Type server library.
 */
[[nodiscard]] RTYPE_SRV_API const char *rtype::srv::version() noexcept {
    return RTYPE_SRV_VER;
}

/**
 * @brief Returns the full version of the R-Type server library.
 * @return The full version of the R-Type server library.
 */
[[nodiscard]] RTYPE_SRV_API std::string rtype::srv::full_version() noexcept {
    return "R-Type server lib Ver. " RTYPE_SRV_VER;
}
