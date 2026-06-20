#include "host/host_cli.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

namespace
{
void expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        throw std::runtime_error(message);
    }
}

vac::host::CommandLine commandLine(std::vector<std::string> args)
{
    std::vector<char *> argv;
    argv.reserve(args.size());
    for (std::string &arg : args) {
        argv.push_back(arg.data());
    }
    return {static_cast<int>(argv.size()), argv.data()};
}
} // namespace

int main()
{
    try {
    {
        auto cli = commandLine({"tool",
                                "--frames", "3",
                                "--ticks", "120",
                                "--scene", "arena.json",
                                "--result-file", "result.json",
                                "--input-script", "input.json",
                                "--command-script", "commands.json",
                                "--log-file", "tool.log",
                                "--seed", "42",
                                "--offline",
                                "--headless",
                                "--hidden-window"});
        const vac::host::CommonHostOptions options = vac::host::parseCommonOptions(cli);
        cli.rejectUnknown();

        expect(options.frames == 3, "frames parse");
        expect(options.ticks == 120, "ticks parse");
        expect(options.scene.has_value() && options.scene->string() == "arena.json", "scene parse");
        expect(options.resultFile.has_value() && options.resultFile->string() == "result.json", "result-file parse");
        expect(options.inputScript.has_value() && options.inputScript->string() == "input.json", "input-script parse");
        expect(options.commandScript.has_value() && options.commandScript->string() == "commands.json", "command-script parse");
        expect(options.logFile.has_value() && options.logFile->string() == "tool.log", "log-file parse");
        expect(options.seed == 42, "seed parse");
        expect(options.offline, "offline parse");
        expect(options.headless, "headless parse");
        expect(options.hiddenWindow, "hidden-window parse");
    }

    {
        auto cli = commandLine({"tool", "--scene"});
        try {
            (void)vac::host::parseCommonOptions(cli);
            expect(false, "missing scene value rejected");
        } catch (const vac::host::ParseError &error) {
            expect(std::string{error.what()}.find("--scene requires a value") != std::string::npos,
                   "missing scene value message");
        }
    }

    {
        auto cli = commandLine({"tool", "--frames", "nope"});
        try {
            (void)vac::host::parseCommonOptions(cli);
            expect(false, "invalid numeric value rejected");
        } catch (const vac::host::ParseError &error) {
            expect(std::string{error.what()}.find("--frames requires a uint32 value") != std::string::npos,
                   "invalid numeric value message");
        }
    }

    {
        auto cli = commandLine({"tool", "--bogus"});
        (void)vac::host::parseCommonOptions(cli);
        try {
            cli.rejectUnknown();
            expect(false, "unknown option rejected");
        } catch (const vac::host::ParseError &error) {
            expect(std::string{error.what()}.find("Unknown option '--bogus'") != std::string::npos,
                   "unknown option message");
        }
    }

    {
        auto cli = commandLine({"tool", "--ticks", "5", "--headless"});
        const vac::host::CommonHostOptions options = vac::host::parseCommonOptions(cli);
        try {
            vac::host::rejectUnsupportedCommonOptions(options, {"--ticks"});
            expect(false, "unsupported common flag rejected");
        } catch (const vac::host::ParseError &error) {
            expect(std::string{error.what()}.find("--headless is not supported") != std::string::npos,
                   "unsupported common flag message");
        }
    }

    {
        const std::filesystem::path path = std::filesystem::temp_directory_path() /
            "vulkan_action_client_host_cli_result.json";
        vac::host::HostResult result;
        result.host = "host_cli_tests";
        result.status = "ok";
        result.message = "done";
        result.frames = 3;
        result.ticks = 12;
        result.seed = 99;
        result.diagnostics.push_back("sample diagnostic");

        vac::host::writeResultFile(path, result);

        {
            std::ifstream input{path};
            expect(input.good(), "result file written");
            const nlohmann::json document = nlohmann::json::parse(input);
            expect(document.at("host").get<std::string>() == "host_cli_tests", "result host");
            expect(document.at("status").get<std::string>() == "ok", "result status");
            expect(document.at("frames").get<int>() == 3, "result frames");
            expect(document.at("ticks").get<int>() == 12, "result ticks");
            expect(document.at("seed").get<int>() == 99, "result seed");
            expect(document.at("diagnostics").at(0).get<std::string>() == "sample diagnostic",
                   "result diagnostics");
        }

        std::filesystem::remove(path);
    }

    return 0;
    } catch (const std::exception &error) {
        std::cerr << "host_cli_tests failed: " << error.what() << '\n';
        return 1;
    }
}
