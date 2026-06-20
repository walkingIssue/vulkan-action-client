#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "content/authoring_scene.hpp"

namespace vac::authoring
{
struct MapCommandDiagnostic
{
    std::string severity = "error";
    std::string code;
    uint32_t commandIndex = 0;
    std::string file;
    std::string objectId;
    std::string component;
    std::string fieldPath;
    std::string message;
};

struct MapCommandRunOptions
{
    std::filesystem::path workingDirectory;
    std::optional<uint32_t> defaultPlayTicks;
};

struct MapCommandResult
{
    std::string status = "ok";
    std::string message;
    uint32_t commandCount = 0;
    std::vector<std::string> createdObjectIds;
    std::vector<MapCommandDiagnostic> diagnostics;
    std::optional<std::filesystem::path> savedPath;
    uint32_t entityCount = 0;
    uint32_t colliderCount = 0;
    uint32_t spawnPointCount = 0;
    uint32_t ticksPlayed = 0;
    uint64_t stateHash = 0;
    content::AuthoringScene scene;

    [[nodiscard]] bool ok() const { return status == "ok"; }
};

MapCommandResult runMapCommandDocument(const nlohmann::json &document, const MapCommandRunOptions &options = {});
MapCommandResult runMapCommandScript(const std::filesystem::path &path, const MapCommandRunOptions &options = {});
nlohmann::json mapCommandResultToJson(const MapCommandResult &result, std::string host = "map_command_runner");
} // namespace vac::authoring
