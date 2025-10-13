#include "SetSigHandlers.hpp"
#include <RTypeSrv/Exception.hpp>
#include <atomic>
#include <csignal>
#include <iostream>

#if !defined(_WIN32)
    #include <cstring>
    #include <sys/types.h>
#endif

namespace rtype::srv {

namespace {
std::atomic sig_count{0};
}// namespace

#if defined(_WIN32)
/**
 * @brief Handles signals on Windows.
 * @param sig The signal number.
 */
void handle_signals(int sig)
#else
/**
 * @brief Handles signals on non-Windows platforms.
 * @param sig The signal number.
 * @param info A pointer to a siginfo_t object.
 * @param ucontext A pointer to a ucontext_t object.
 */
void handle_signals(const int sig, siginfo_t *, void *)
#endif
{
    if (sig == SIGINT) {
        std::cout << "\r" << std::flush;
    }
    if (sig_count == 0) {
        std::cout << "Stopping server..." << std::endl;
    }
    if (++sig_count >= max_sig_count) {
        std::cerr << "More than " << max_sig_count << " SIGINTs detected... Force stopping server..." << std::endl;
        std::abort();
    }
    quitServer = true;
}

/**
 * @brief Sets the signal handlers for the server.
 */
void setSigHandlers() noexcept
{
#if defined(_WIN32)
    std::signal(SIGINT, handle_signals);
    std::signal(SIGTERM, handle_signals);
#else
    struct sigaction sa{};
    sa.sa_sigaction = handle_signals;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
#endif
}

}// namespace rtype::srv
