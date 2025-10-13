#include "GetConfig.hpp"
#include "SetSigHandlers.hpp"
#include "StartServer.hpp"
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

std::atomic<bool> quitServer = false; ///< An atomic boolean to signal the server to quit.

/**
 * @brief Gets the path to the configuration file from the binary's path.
 * @param argv0 The first argument passed to the program.
 * @param configFileName The name of the configuration file.
 * @return The path to the configuration file.
 */
static std::string getConfigPathFromBin(const char *argv0, const std::string &configFileName)
{
    const std::filesystem::path exePath = std::filesystem::canonical(argv0);
    return (exePath.parent_path() / configFileName).string();
}

/**
 * @brief Prints the help message.
 */
static void printHelp() noexcept
{
    std::cout << "Usage: ./r-type_server -h host -p port -j n_cores" << std::endl;
}

/**
 * @brief Parses the command-line arguments.
 * @param ac The number of arguments.
 * @param av An array of strings representing the arguments.
 * @param cfgFile A reference to a string to store the configuration file path in.
 * @return 0 on success, 1 on help request, 84 on error.
 */
static int parseArgs(const int ac, const char *av[], std::string &cfgFile)
{
    for (int i = 1; i < ac; ++i) {
        if (std::strcmp(av[i], "-h") == 0) {
            return 1;
        }
        if (std::strcmp(av[i], "-c") == 0 && i + 1 < ac) {
            cfgFile = av[i + 1];
            ++i;
        } else {
            std::cerr << "Invalid argument: " << av[i] << std::endl;
            return 84;
        }
    }
    return 0;
}

/**
 * @brief Creates all the threads for the server.
 * @param cfg The configuration to use.
 */
static void makeAllThreads(const rtype::srv::Config &cfg)
{
    std::vector<std::thread> threads;

    threads.reserve((cfg.tcp_only ? 0 : cfg.n_cores) + (cfg.udp_only ? 0 : 1));
    if (!cfg.udp_only) {
        threads.emplace_back(rtype::srv::startTcpServer(cfg.tcp_endpoint, quitServer));
        if (!cfg.tcp_only) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<std::size_t>(1e9)));
        }
    }
    if (!cfg.tcp_only) {
        for (auto &thread : rtype::srv::startUdpServers(cfg.udp_endpoint, cfg.n_cores, cfg.tcp_endpoint, cfg.external_udp_endpoint, quitServer)) {
            threads.emplace_back(std::move(thread));
        }
    }
    for (auto &t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

/**
 * @brief The main function of the program.
 * @param ac The number of arguments.
 * @param av An array of strings representing the arguments.
 * @return 0 on success, 84 on error.
 */
int main(const int ac, const char *av[]) noexcept
{
    rtype::srv::Config cfg{};
    std::string cfgFile = getConfigPathFromBin(av[0], "r-type_server.cfg");

    if (int ret = 0; ac > 1 && ((ret = parseArgs(ac, av, cfgFile)))) {
        printHelp();
        if (ret == 84) {
            return 84;
        }
        return 0;
    }
    try {
        cfg = rtype::srv::getConfig(cfgFile);
    } catch (const std::exception &e) {
        std::cerr << "Error reading config file: " << e.what() << std::endl;
        return 84;
    }
    rtype::srv::setSigHandlers();
    makeAllThreads(cfg);
    return 0;
}
