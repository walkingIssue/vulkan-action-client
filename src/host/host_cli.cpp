#include "host/host_cli.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <system_error>
#include <unordered_set>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace vac::host
{
namespace
{
uint32_t parseUint32(std::string_view optionName, const std::string &value)
{
    try {
        size_t consumed = 0;
        const unsigned long parsed = std::stoul(value, &consumed, 10);
        if (consumed == value.size() && parsed <= std::numeric_limits<uint32_t>::max()) {
            return static_cast<uint32_t>(parsed);
        }
    } catch (const std::exception &) {
    }

    throw ParseError(fmt::format("{} requires a uint32 value, got '{}'", optionName, value));
}

uint64_t parseUint64(std::string_view optionName, const std::string &value)
{
    try {
        size_t consumed = 0;
        const unsigned long long parsed = std::stoull(value, &consumed, 10);
        if (consumed == value.size()) {
            return static_cast<uint64_t>(parsed);
        }
    } catch (const std::exception &) {
    }

    throw ParseError(fmt::format("{} requires a uint64 value, got '{}'", optionName, value));
}

void markSpecified(CommonHostOptions &options, std::string_view optionName)
{
    options.specifiedOptions.emplace_back(optionName);
}

nlohmann::json toJson(const HostResult &result)
{
    nlohmann::json document{
        {"host", result.host},
        {"status", result.status},
        {"message", result.message},
        {"diagnostics", result.diagnostics},
    };
    if (result.frames.has_value()) {
        document["frames"] = *result.frames;
    }
    if (result.ticks.has_value()) {
        document["ticks"] = *result.ticks;
    }
    if (result.seed.has_value()) {
        document["seed"] = *result.seed;
    }
    return document;
}
} // namespace

ParseError::ParseError(const std::string &message)
    : std::runtime_error(message)
{
}

CommandLine::CommandLine(int argc, char **argv)
{
    m_args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        m_args.emplace_back(argv[i]);
    }
    m_consumed.assign(m_args.size(), false);
    if (!m_consumed.empty()) {
        m_consumed[0] = true;
    }
}

std::string_view CommandLine::executableName() const
{
    return m_args.empty() ? std::string_view{} : std::string_view{m_args.front()};
}

bool CommandLine::consumeFlag(std::string_view name)
{
    for (size_t i = 1; i < m_args.size(); ++i) {
        if (!m_consumed[i] && m_args[i] == name) {
            m_consumed[i] = true;
            return true;
        }
    }
    return false;
}

std::optional<std::string> CommandLine::consumeValue(std::string_view name)
{
    for (size_t i = 1; i < m_args.size(); ++i) {
        if (m_consumed[i] || m_args[i] != name) {
            continue;
        }
        if (i + 1 >= m_args.size() || m_args[i + 1].starts_with("--")) {
            throw ParseError(fmt::format("{} requires a value", name));
        }

        m_consumed[i] = true;
        m_consumed[i + 1] = true;
        return m_args[i + 1];
    }
    return std::nullopt;
}

void CommandLine::rejectUnknown() const
{
    for (size_t i = 1; i < m_args.size(); ++i) {
        if (!m_consumed[i]) {
            throw ParseError(fmt::format("Unknown option '{}'", m_args[i]));
        }
    }
}

CommonHostOptions parseCommonOptions(CommandLine &commandLine)
{
    CommonHostOptions options;
    if (const std::optional<std::string> value = commandLine.consumeValue("--frames")) {
        options.frames = parseUint32("--frames", *value);
        markSpecified(options, "--frames");
    }
    if (const std::optional<std::string> value = commandLine.consumeValue("--ticks")) {
        options.ticks = parseUint32("--ticks", *value);
        markSpecified(options, "--ticks");
    }
    if (const std::optional<std::string> value = commandLine.consumeValue("--scene")) {
        options.scene = *value;
        markSpecified(options, "--scene");
    }
    if (const std::optional<std::string> value = commandLine.consumeValue("--result-file")) {
        options.resultFile = *value;
        markSpecified(options, "--result-file");
    }
    if (const std::optional<std::string> value = commandLine.consumeValue("--input-script")) {
        options.inputScript = *value;
        markSpecified(options, "--input-script");
    }
    if (const std::optional<std::string> value = commandLine.consumeValue("--command-script")) {
        options.commandScript = *value;
        markSpecified(options, "--command-script");
    }
    if (const std::optional<std::string> value = commandLine.consumeValue("--log-file")) {
        options.logFile = *value;
        markSpecified(options, "--log-file");
    }
    if (const std::optional<std::string> value = commandLine.consumeValue("--seed")) {
        options.seed = parseUint64("--seed", *value);
        markSpecified(options, "--seed");
    }
    if (commandLine.consumeFlag("--offline")) {
        options.offline = true;
        markSpecified(options, "--offline");
    }
    if (commandLine.consumeFlag("--headless")) {
        options.headless = true;
        markSpecified(options, "--headless");
    }
    if (commandLine.consumeFlag("--hidden-window")) {
        options.hiddenWindow = true;
        markSpecified(options, "--hidden-window");
    }
    return options;
}

void rejectUnsupportedCommonOptions(const CommonHostOptions &options,
                                    const std::vector<std::string_view> &supportedOptions)
{
    std::unordered_set<std::string_view> supported;
    for (std::string_view option : supportedOptions) {
        supported.insert(option);
    }

    for (const std::string &specified : options.specifiedOptions) {
        if (!supported.contains(specified)) {
            throw ParseError(fmt::format("{} is not supported by this executable yet", specified));
        }
    }
}

void writeResultFile(const std::filesystem::path &path, const HostResult &result)
{
    if (path.empty()) {
        return;
    }

    const std::filesystem::path absolutePath = std::filesystem::absolute(path);
    std::error_code error;
    const std::filesystem::path parent = absolutePath.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            throw std::runtime_error(fmt::format("Could not create result directory '{}': {}",
                                                 parent.string(),
                                                 error.message()));
        }
    }

    const std::filesystem::path temporaryPath = absolutePath.string() + ".tmp";
    {
        std::ofstream output{temporaryPath, std::ios::trunc};
        if (!output) {
            throw std::runtime_error(fmt::format("Could not open result file '{}'", temporaryPath.string()));
        }
        output << toJson(result).dump(2) << '\n';
    }

    std::filesystem::remove(absolutePath, error);
    error.clear();
    std::filesystem::rename(temporaryPath, absolutePath, error);
    if (error) {
        throw std::runtime_error(fmt::format("Could not replace result file '{}': {}",
                                             absolutePath.string(),
                                             error.message()));
    }
}

HostResult resultFromOptions(std::string host, const CommonHostOptions &options)
{
    HostResult result;
    result.host = std::move(host);
    result.frames = options.frames;
    result.ticks = options.ticks;
    result.seed = options.seed;
    return result;
}
} // namespace vac::host
