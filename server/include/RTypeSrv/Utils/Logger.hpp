#include <RTypeSrv/Utils/Singleton.hpp>
#include <iostream>
#include <mutex>
#include <utility>

namespace rtype::srv::utils {

/**
 * @brief A simple logger class.
 *
 * This class is a singleton, so there can only be one instance of it.
 * It provides two methods, `cout` and `cerr`, which are thread-safe
 * versions of `std::cout` and `std::cerr`.
 */
class Logger : public Singleton<Logger>
{
        friend class Singleton;

    public:
        /**
         * @brief Prints a message to standard output.
         * @param args The arguments to print.
         */
        template<typename... Args>
        void cout(Args &&...args)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            (std::cout << ... << args) << std::endl;
        }

        /**
         * @brief Prints a message to standard error.
         * @param args The arguments to print.
         */
        template<typename... Args>
        void cerr(Args &&...args)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            (std::cerr << ... << args) << std::endl;
        }

    protected:
        constexpr explicit Logger() = default;
        ~Logger() noexcept = default;

    private:
        std::mutex _mtx;
};

/**
 * @brief Prints a message to standard output.
 * @param args The arguments to print.
 */
template<typename... Args>
static inline void cout(Args &&...args)
{
    Logger::getInstance().cout(std::forward<Args>(args)...);
}

/**
 * @brief Prints a message to standard error.
 * @param args The arguments to print.
 */
template<typename... Args>
static inline void cerr(Args &&...args)
{
    Logger::getInstance().cerr(std::forward<Args>(args)...);
}

}// namespace rtype::srv::utils
