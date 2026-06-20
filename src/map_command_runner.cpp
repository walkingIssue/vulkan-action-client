#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <system_error>

#include "authoring/map_command_script.hpp"
#include "host/host_cli.hpp"

namespace
{
void writeJsonFile(const std::filesystem::path &path, const nlohmann::json &document)
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
            throw std::runtime_error("Could not create result directory '" + parent.string() + "': " + error.message());
        }
    }

    const std::filesystem::path temporaryPath = absolutePath.string() + ".tmp";
    {
        std::ofstream output{temporaryPath, std::ios::trunc};
        if (!output) {
            throw std::runtime_error("Could not open result file '" + temporaryPath.string() + "'");
        }
        output << document.dump(2) << '\n';
    }

    std::filesystem::remove(absolutePath, error);
    error.clear();
    std::filesystem::rename(temporaryPath, absolutePath, error);
    if (error) {
        throw std::runtime_error("Could not replace result file '" + absolutePath.string() + "': " + error.message());
    }
}
} // namespace

int main(int argc, char **argv)
{
    std::optional<std::filesystem::path> resultFile;
    try {
        vac::host::CommandLine commandLine{argc, argv};
        const vac::host::CommonHostOptions commonOptions = vac::host::parseCommonOptions(commandLine);
        resultFile = commonOptions.resultFile;
        vac::host::rejectUnsupportedCommonOptions(
            commonOptions, {"--command-script", "--result-file", "--ticks", "--headless", "--offline", "--seed"});
        commandLine.rejectUnknown();

        if (!commonOptions.commandScript.has_value()) {
            throw vac::host::ParseError{"--command-script is required"};
        }

        vac::authoring::MapCommandRunOptions options;
        options.workingDirectory = std::filesystem::current_path();
        options.defaultPlayTicks = commonOptions.ticks;

        const vac::authoring::MapCommandResult result =
            vac::authoring::runMapCommandScript(*commonOptions.commandScript, options);
        const nlohmann::json resultJson = vac::authoring::mapCommandResultToJson(result);
        if (resultFile.has_value()) {
            writeJsonFile(*resultFile, resultJson);
        } else {
            std::cout << resultJson.dump(2) << '\n';
        }
        return result.ok() ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << "map_command_runner failed: " << error.what() << '\n';
        if (resultFile.has_value()) {
            try {
                nlohmann::json result{
                    {"host", "map_command_runner"},
                    {"status", "error"},
                    {"message", error.what()},
                    {"diagnostics", nlohmann::json::array({{{"severity", "error"},
                                                            {"code", "process_error"},
                                                            {"commandIndex", 0},
                                                            {"fieldPath", ""},
                                                            {"message", error.what()}}})},
                };
                writeJsonFile(*resultFile, result);
            } catch (const std::exception &resultError) {
                std::cerr << "map_command_runner could not write result file: " << resultError.what() << '\n';
            }
        }
        return 1;
    }
}
