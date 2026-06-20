#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vac::host
{
class ParseError : public std::runtime_error
{
public:
    explicit ParseError(const std::string &message);
};

class CommandLine
{
public:
    CommandLine(int argc, char **argv);

    [[nodiscard]] std::string_view executableName() const;
    [[nodiscard]] bool consumeFlag(std::string_view name);
    [[nodiscard]] std::optional<std::string> consumeValue(std::string_view name);
    void rejectUnknown() const;

private:
    std::vector<std::string> m_args;
    std::vector<bool> m_consumed;
};

struct CommonHostOptions
{
    std::optional<uint32_t> frames;
    std::optional<uint32_t> ticks;
    std::optional<std::filesystem::path> scene;
    std::optional<std::filesystem::path> resultFile;
    std::optional<std::filesystem::path> inputScript;
    std::optional<std::filesystem::path> commandScript;
    std::optional<std::filesystem::path> logFile;
    std::optional<uint64_t> seed;
    bool offline = false;
    bool headless = false;
    bool hiddenWindow = false;
    std::vector<std::string> specifiedOptions;
};

struct HostResult
{
    std::string host;
    std::string status = "ok";
    std::string message;
    std::optional<uint32_t> frames;
    std::optional<uint32_t> ticks;
    std::optional<uint64_t> seed;
    std::vector<std::string> diagnostics;
};

CommonHostOptions parseCommonOptions(CommandLine &commandLine);
void rejectUnsupportedCommonOptions(const CommonHostOptions &options,
                                    const std::vector<std::string_view> &supportedOptions);
void writeResultFile(const std::filesystem::path &path, const HostResult &result);
HostResult resultFromOptions(std::string host, const CommonHostOptions &options);
} // namespace vac::host
