#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "authoring/map_command_script.hpp"
#include "content/authoring_scene.hpp"

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

std::filesystem::path fixturePath()
{
    return projectRoot() / "tests/fixtures/commands/wire_arena.commands.json";
}

void expect(bool condition, std::string_view message)
{
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

nlohmann::json readJson(const std::filesystem::path &path)
{
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Could not open " + path.string());
    }

    nlohmann::json document;
    input >> document;
    return document;
}

vac::authoring::MapCommandResult runFixture()
{
    vac::authoring::MapCommandRunOptions options;
    options.workingDirectory = std::filesystem::current_path();
    return vac::authoring::runMapCommandScript(fixturePath(), options);
}

void fixtureCreatesSavesReopensCompilesAndPlays()
{
    const vac::authoring::MapCommandResult result = runFixture();
    expect(result.ok(), "wire arena command script succeeds");
    expect(result.commandCount == 17, "wire arena command count");
    expect(result.createdObjectIds.size() == 2, "wire arena created object count");
    expect(result.createdObjectIds.at(0) == "arena_floor", "wire arena first created id");
    expect(result.createdObjectIds.at(1) == "center_blocker", "wire arena second created id");
    expect(result.savedPath.has_value(), "wire arena saved path recorded");
    expect(result.savedPath.has_value() && std::filesystem::exists(*result.savedPath), "wire arena saved file exists");
    expect(result.entityCount == 2, "wire arena compiled entity count");
    expect(result.colliderCount == 2, "wire arena compiled collider count");
    expect(result.spawnPointCount == 2, "wire arena compiled spawn count");
    expect(result.ticksPlayed == 300, "wire arena plays requested ticks");
    expect(result.stateHash != 0, "wire arena final state hash is nonzero");
    expect(result.diagnostics.empty(), "wire arena has no diagnostics");

    const vac::content::AuthoringScene saved = vac::content::loadAuthoringScene(*result.savedPath);
    expect(vac::content::validateAuthoringScene(saved).ok(), "wire arena saved map validates");
    expect(vac::content::toCanonicalJsonString(saved) == vac::content::toCanonicalJsonString(result.scene),
           "wire arena saved map matches final canonical document");
}

void commandExecutionIsDeterministic()
{
    const vac::authoring::MapCommandResult first = runFixture();
    const vac::authoring::MapCommandResult second = runFixture();
    expect(first.ok() && second.ok(), "determinism runs both succeed");
    expect(first.createdObjectIds == second.createdObjectIds, "created ids are deterministic");
    expect(first.entityCount == second.entityCount, "entity count is deterministic");
    expect(first.colliderCount == second.colliderCount, "collider count is deterministic");
    expect(first.spawnPointCount == second.spawnPointCount, "spawn count is deterministic");
    expect(first.stateHash == second.stateHash, "state hash is deterministic");
    expect(vac::content::toCanonicalJsonString(first.scene) == vac::content::toCanonicalJsonString(second.scene),
           "canonical map output is deterministic");
}

void invalidCommandReportsStructuredDiagnostic()
{
    const nlohmann::json document = nlohmann::json::array({
        {{"command", "newFromTemplate"}, {"template", "scene.template.basic_render"}},
        {{"command", "unknownCommand"}},
    });

    vac::authoring::MapCommandRunOptions options;
    options.workingDirectory = std::filesystem::current_path();
    const vac::authoring::MapCommandResult result = vac::authoring::runMapCommandDocument(document, options);
    expect(!result.ok(), "invalid command fails");
    expect(!result.diagnostics.empty(), "invalid command produces diagnostics");
    expect(!result.diagnostics.empty() && result.diagnostics.front().code == "unknown_command",
           "invalid command diagnostic code");
    expect(!result.diagnostics.empty() && result.diagnostics.front().commandIndex == 1,
           "invalid command diagnostic index");
}

void resultJsonIncludesAuthoringFields()
{
    const vac::authoring::MapCommandResult result = runFixture();
    const nlohmann::json document = vac::authoring::mapCommandResultToJson(result, "map_command_runner_tests");
    expect(document.at("host").get<std::string>() == "map_command_runner_tests", "result json host");
    expect(document.at("status").get<std::string>() == "ok", "result json status");
    expect(document.at("createdObjectIds").size() == 2, "result json created ids");
    expect(document.at("diagnostics").is_array(), "result json diagnostics array");
    expect(document.at("ticks").get<uint32_t>() == 300, "result json ticks");
    expect(document.at("stateHash").get<uint64_t>() == result.stateHash, "result json state hash");
    expect(document.contains("savedPath"), "result json saved path");
}
} // namespace

int main()
{
    try {
        fixtureCreatesSavesReopensCompilesAndPlays();
        commandExecutionIsDeterministic();
        invalidCommandReportsStructuredDiagnostic();
        resultJsonIncludesAuthoringFields();
    } catch (const std::exception &error) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }

    return g_failures == 0 ? 0 : 1;
}
