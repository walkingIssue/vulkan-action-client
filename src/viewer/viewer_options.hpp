#pragma once

#include "host/host_cli.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vac::viewer
{
struct ViewerOptions
{
    std::filesystem::path scenePath;
    std::filesystem::path controlProfilePath;
    std::string networkServer = "127.0.0.1:40000";
    uint8_t networkClientId = 0;
    uint32_t frames = 0;
    bool orbitCamera = false;
    bool visualLab = false;
    std::filesystem::path visualLabMovePath;
    std::filesystem::path visualLabProxyAnimationPath;
    std::filesystem::path visualLabScenarioPath;
    host::CommonHostOptions common;
};

ViewerOptions parseOptions(int argc, char **argv);

void writeSuccessResultFile(const ViewerOptions &options,
                            const std::vector<std::string> &diagnostics);

void writeErrorResultFile(const std::filesystem::path &resultFile,
                          const std::string &message);
} // namespace vac::viewer
