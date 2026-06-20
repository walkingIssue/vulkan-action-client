#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

#include "combat/combat_scenario.hpp"
#include "host/host_cli.hpp"

namespace
{
bool goldenUpdateAllowed()
{
    const char *value = std::getenv("VAC_UPDATE_GOLDENS");
    return value != nullptr && std::string_view{value} == "1";
}
} // namespace

int main(int argc, char **argv)
{
    std::optional<std::filesystem::path> resultFile;
    try {
        vac::host::CommandLine commandLine{argc, argv};
        const vac::host::CommonHostOptions commonOptions = vac::host::parseCommonOptions(commandLine);
        resultFile = commonOptions.resultFile;
        vac::host::rejectUnsupportedCommonOptions(commonOptions, {"--ticks", "--result-file", "--headless", "--seed"});

        const std::optional<std::string> scenarioValue = commandLine.consumeValue("--scenario");
        const bool updateGolden = commandLine.consumeFlag("--update-golden");
        commandLine.rejectUnknown();

        if (!scenarioValue.has_value()) {
            throw vac::host::ParseError("--scenario requires a value");
        }

        vac::combat::ScenarioRunOptions options;
        options.overrideTicks = commonOptions.ticks;
        options.overrideSeed = commonOptions.seed;
        options.updateGolden = updateGolden;
        options.allowGoldenUpdate = goldenUpdateAllowed();

        vac::combat::CombatScenario scenario = vac::combat::loadCombatScenario(*scenarioValue);
        vac::combat::ScenarioRunResult result = vac::combat::runCombatScenario(std::move(scenario), options);
        if (resultFile.has_value()) {
            vac::combat::writeScenarioResultFile(*resultFile, result);
        }

        std::cout << result.message << '\n';
        return result.status == "ok" ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << "combat_scenario_runner failed: " << error.what() << '\n';
        if (resultFile.has_value()) {
            try {
                vac::combat::ScenarioRunResult result;
                result.status = "error";
                result.message = error.what();
                vac::combat::writeScenarioResultFile(*resultFile, result);
            } catch (const std::exception &resultError) {
                std::cerr << "combat_scenario_runner could not write result file: " << resultError.what() << '\n';
            }
        }
        return 1;
    }
}
