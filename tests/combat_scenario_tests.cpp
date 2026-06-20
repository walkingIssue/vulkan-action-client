#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

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

bool hasDiagnostic(const vac::combat::ScenarioRunResult &result, std::string_view code, std::string_view fieldPath)
{
    for (const vac::combat::ScenarioDiagnostic &diagnostic : result.diagnostics) {
        if (diagnostic.code == code && diagnostic.fieldPath == fieldPath) {
            return true;
        }
    }
    return false;
}

void expectDiagnostic(const vac::combat::ScenarioRunResult &result, std::string_view code, std::string_view fieldPath)
{
    expect(hasDiagnostic(result, code, fieldPath),
           "diagnostic " + std::string{code} + " at " + std::string{fieldPath});
}

std::vector<const vac::combat::ScenarioTraceEvent *> eventsOfKind(const vac::combat::ScenarioRunResult &result,
                                                                  std::string_view kind)
{
    std::vector<const vac::combat::ScenarioTraceEvent *> events;
    for (const vac::combat::ScenarioTraceEvent &event : result.trace.events) {
        if (event.kind == kind) {
            events.push_back(&event);
        }
    }
    return events;
}

std::filesystem::path writeMoveWithHitboxSocket(std::string_view socketName)
{
    const std::filesystem::path source = projectRoot() / "content/moves/light_attack.move.json";
    std::ifstream input{source};
    if (!input) {
        throw std::runtime_error("Could not open source move " + source.string());
    }

    nlohmann::json document;
    input >> document;
    document.at("hitboxTracks").at(0).at("socket") = std::string{socketName};

    const std::filesystem::path outputPath = std::filesystem::temp_directory_path() / "vac_rm1_003_bad_socket.move.json";
    std::ofstream output{outputPath, std::ios::trunc};
    if (!output) {
        throw std::runtime_error("Could not write temporary move " + outputPath.string());
    }
    output << document.dump(2) << '\n';
    return outputPath;
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

void missingContentPathsReportStructuredDiagnostics()
{
    vac::combat::CombatScenario scenario =
        vac::combat::loadCombatScenario(scenarioPath("sword_light_hits_idle_target.scenario.json"));
    scenario.mapPath = "content/maps/rm1_missing.map.json";
    scenario.movePaths[0] = "content/moves/rm1_missing.move.json";
    scenario.animations[0].path = "content/animations/rm1_missing.proxy_anim.json";
    scenario.goldenPath = "tests/fixtures/scenarios/goldens/rm1_missing.golden.json";

    const vac::combat::ScenarioRunResult result = vac::combat::runCombatScenario(std::move(scenario));
    expect(result.status == "error", "missing content graph exits error");
    expect(result.message == "Scenario content graph validation failed", "missing content graph message");
    expectDiagnostic(result, "missing_map_file", "map");
    expectDiagnostic(result, "missing_move_file", "moves/0");
    expectDiagnostic(result, "missing_proxy_animation_file", "animations/0/path");
    expectDiagnostic(result, "missing_golden_file", "golden");
    expect(result.trace.events.empty(), "missing content graph exits before simulation");
}

void unknownAnimationBindingReportsMoveDiagnostic()
{
    vac::combat::CombatScenario scenario =
        vac::combat::loadCombatScenario(scenarioPath("sword_light_hits_idle_target.scenario.json"));
    scenario.animations.push_back({
        "move.unknown_rm1",
        "content/animations/sword_light_proxy.anim.json",
    });

    const vac::combat::ScenarioRunResult result = vac::combat::runCombatScenario(std::move(scenario));
    expect(result.status == "error", "unknown animation binding exits error");
    expectDiagnostic(result, "unknown_animation_move", "animations/2/move");
    expect(result.trace.events.empty(), "unknown animation binding exits before simulation");
}

void hitboxMoveRequiresProxyAnimationBinding()
{
    vac::combat::CombatScenario scenario =
        vac::combat::loadCombatScenario(scenarioPath("sword_light_hits_idle_target.scenario.json"));
    scenario.animations.erase(scenario.animations.begin());

    const vac::combat::ScenarioRunResult result = vac::combat::runCombatScenario(std::move(scenario));
    expect(result.status == "error", "missing move animation binding exits error");
    expectDiagnostic(result, "missing_move_animation", "moves/0");
    expect(result.trace.events.empty(), "missing move animation binding exits before simulation");
}

void hitboxSocketMustExistInBoundProxyAnimation()
{
    vac::combat::CombatScenario scenario =
        vac::combat::loadCombatScenario(scenarioPath("sword_light_hits_idle_target.scenario.json"));
    scenario.movePaths[0] = writeMoveWithHitboxSocket("rm1_missing_socket");

    const vac::combat::ScenarioRunResult result = vac::combat::runCombatScenario(std::move(scenario));
    expect(result.status == "error", "missing hitbox socket exits error");
    expectDiagnostic(result, "missing_hitbox_socket", "moves/move.light_attack/hitboxTracks/blade_arc/socket");
    expect(result.trace.events.empty(), "missing hitbox socket exits before simulation");
}

void hitTraceIncludesDamageAndReactionFields()
{
    const vac::combat::ScenarioRunResult result = runScenario("sword_light_hits_idle_target.scenario.json");
    const std::vector<const vac::combat::ScenarioTraceEvent *> hitEvents = eventsOfKind(result, "hit");
    expect(hitEvents.size() == 1, "single hit event emitted");
    if (hitEvents.empty()) {
        return;
    }

    const vac::combat::ScenarioTraceEvent &hit = *hitEvents.front();
    expect(hit.hasEffect, "hit event carries effect fields");
    expect(hit.damage == 12, "hit event records damage");
    expect(hit.targetRemainingHealth == 88, "hit event records remaining health");
    expect(hit.reactionMove == "move.hit_reaction", "hit event records reaction move");
    expect(hit.hitstopTicks == 1, "hit event records hitstop");
    expect(hit.stunTicks == 3, "hit event records stun");
}

void blockedHitTraceShowsNoDamage()
{
    const vac::combat::ScenarioRunResult result = runScenario("dodge_invulnerability_boundary.scenario.json");
    const std::vector<const vac::combat::ScenarioTraceEvent *> blockedEvents = eventsOfKind(result, "hit_blocked");
    expect(!blockedEvents.empty(), "blocked hit events emitted");
    if (!blockedEvents.empty()) {
        const vac::combat::ScenarioTraceEvent &blocked = *blockedEvents.front();
        expect(blocked.hasEffect, "blocked hit carries no-damage effect fields");
        expect(blocked.damage == 0, "blocked hit records zero damage");
        expect(blocked.targetRemainingHealth == 100, "blocked hit preserves health");
        expect(blocked.reactionMove.empty(), "blocked hit does not start reaction");
    }

    const std::vector<const vac::combat::ScenarioTraceEvent *> hitEvents = eventsOfKind(result, "hit");
    expect(hitEvents.size() == 1, "post-invulnerability hit applies exactly once");
    if (!hitEvents.empty()) {
        expect(hitEvents.front()->damage == 12, "post-invulnerability hit applies damage");
        expect(hitEvents.front()->targetRemainingHealth == 88, "post-invulnerability hit leaves expected health");
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
        missingContentPathsReportStructuredDiagnostics();
        unknownAnimationBindingReportsMoveDiagnostic();
        hitboxMoveRequiresProxyAnimationBinding();
        hitboxSocketMustExistInBoundProxyAnimation();
        hitTraceIncludesDamageAndReactionFields();
        blockedHitTraceShowsNoDamage();
    } catch (const std::exception &error) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }

    return g_failures == 0 ? 0 : 1;
}
