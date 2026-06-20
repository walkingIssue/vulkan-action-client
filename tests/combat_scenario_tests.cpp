#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

#include "combat/combat_scenario.hpp"

namespace
{
int g_failures = 0;

std::filesystem::path projectRoot()
{
#ifdef VULKAN_ACTION_CLIENT_PROJECT_ROOT
    return std::filesystem::path{VULKAN_ACTION_CLIENT_PROJECT_ROOT};
#else
    return std::filesystem::current_path();
#endif
}

void expect(bool condition, std::string_view message)
{
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::filesystem::path scenarioPath(std::string_view fileName)
{
    return projectRoot() / "tests/fixtures/scenarios" / fileName;
}

vac::combat::ScenarioRunResult runScenario(std::string_view fileName,
                                           vac::combat::ScenarioRunOptions options = {})
{
    return vac::combat::runCombatScenario(vac::combat::loadCombatScenario(scenarioPath(fileName)), options);
}

void goldenScenariosMatch()
{
    const std::vector<std::string_view> scenarios = {
        "sword_light_hits_idle_target.scenario.json",
        "sword_light_whiffs.scenario.json",
        "sword_light_cancel_on_hit.scenario.json",
        "dodge_invulnerability_boundary.scenario.json",
    };

    for (std::string_view scenario : scenarios) {
        const vac::combat::ScenarioRunResult result = runScenario(scenario);
        expect(result.status == "ok", std::string{scenario} + " exits ok");
        expect(result.goldenCompared && result.goldenMatched, std::string{scenario} + " matches golden");
        expect(!result.trace.finalStateHash.empty(), std::string{scenario} + " writes final state hash");
        expect(!result.trace.events.empty(), std::string{scenario} + " emits trace events");
    }
}

void repeatedRunsAreDeterministic()
{
    const vac::combat::ScenarioRunResult first = runScenario("sword_light_hits_idle_target.scenario.json");
    const vac::combat::ScenarioRunResult second = runScenario("sword_light_hits_idle_target.scenario.json");
    expect(vac::combat::toJson(first.trace) == vac::combat::toJson(second.trace),
           "repeated scenario runs produce identical traces");
}

void guardedGoldenUpdateRequiresExplicitPermission()
{
    vac::combat::ScenarioRunOptions options;
    options.updateGolden = true;
    options.allowGoldenUpdate = false;
    const vac::combat::ScenarioRunResult result = runScenario("sword_light_hits_idle_target.scenario.json", options);
    expect(result.status == "error", "golden update without guard fails");
    expect(!result.goldenMatched, "unguarded golden update does not report matched");
}

void goldenMismatchReportsExpectedAndActual()
{
    vac::combat::CombatScenario scenario =
        vac::combat::loadCombatScenario(scenarioPath("sword_light_hits_idle_target.scenario.json"));
    scenario.goldenPath = "tests/fixtures/scenarios/goldens/sword_light_whiffs.golden.json";

    const vac::combat::ScenarioRunResult result = vac::combat::runCombatScenario(std::move(scenario));
    expect(result.status == "error", "mismatched golden fails");
    expect(!result.diagnostics.empty(), "mismatched golden writes diagnostics");
    if (!result.diagnostics.empty()) {
        const std::string &message = result.diagnostics.front().message;
        expect(message.find("expected=") != std::string::npos, "mismatch diagnostic includes expected event");
        expect(message.find("actual=") != std::string::npos, "mismatch diagnostic includes actual event");
        expect(message.find("tick") != std::string::npos, "mismatch diagnostic includes tick context");
    }
}
} // namespace

int main()
{
    try {
        goldenScenariosMatch();
        repeatedRunsAreDeterministic();
        guardedGoldenUpdateRequiresExplicitPermission();
        goldenMismatchReportsExpectedAndActual();
    } catch (const std::exception &error) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }

    return g_failures == 0 ? 0 : 1;
}
