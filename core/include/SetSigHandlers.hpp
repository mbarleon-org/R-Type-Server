#pragma once
#include <atomic>
#include <csignal>
#include <cstddef>
#include <mutex>

extern std::atomic<bool> quitServer;

namespace rtype::srv {

#ifdef _WIN32
/**
 * @brief Handles signals on Windows.
 * @param sig The signal number.
 */
void handle_signals(int sig);
#else
/**
 * @brief Handles signals on non-Windows platforms.
 * @param sig The signal number.
 * @param info A pointer to a siginfo_t object.
 * @param ucontext A pointer to a ucontext_t object.
 */
void handle_signals(int sig, [[maybe_unused]] siginfo_t *info, [[maybe_unused]] void *ucontext);
#endif

static constexpr int max_sig_count = 3;

/**
 * @brief Sets the signal handlers for the server.
 */
void setSigHandlers() noexcept;

}// namespace rtype::srv
