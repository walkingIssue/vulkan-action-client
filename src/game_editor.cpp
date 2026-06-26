#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <fmt/format.h>
#include <glm/common.hpp>
#include <nlohmann/json.hpp>

#include "animation/proxy_animation.hpp"
#include "authoring/map_command_script.hpp"
#include "combat/combat_scenario.hpp"
#include "content/authoring_scene.hpp"
#include "content/move_asset.hpp"
#include "host/host_cli.hpp"
#include "scene/scene_runtime.hpp"
#include "visual_lab/visual_lab.hpp"

namespace {
constexpr uint32_t kEditorWidth = 1500;
constexpr uint32_t kEditorHeight = 900;

std::filesystem::path projectRoot() {
#ifdef VULKAN_ACTION_CLIENT_PROJECT_ROOT
  return std::filesystem::path{VULKAN_ACTION_CLIENT_PROJECT_ROOT};
#else
  return std::filesystem::current_path();
#endif
}

std::string displayPath(const std::filesystem::path &path) {
  if (path.empty()) {
    return {};
  }

  std::error_code error;
  const std::filesystem::path relative =
      std::filesystem::relative(path, projectRoot(), error);
  if (!error && !relative.empty()) {
    return relative.generic_string();
  }
  return path.generic_string();
}

std::filesystem::path absoluteFromProject(const std::filesystem::path &path) {
  if (path.empty() || path.is_absolute()) {
    return path;
  }
  return projectRoot() / path;
}

nlohmann::json loadJsonFile(const std::filesystem::path &path) {
  std::ifstream input{path};
  if (!input) {
    throw std::runtime_error(fmt::format("Could not open '{}'", path.string()));
  }

  nlohmann::json document;
  input >> document;
  return document;
}

std::string diagnosticLine(const vac::content::ContentDiagnostic &diagnostic) {
  return fmt::format("{}:{} {} {}", diagnostic.severity, diagnostic.code,
                     diagnostic.fieldPath, diagnostic.message);
}

std::string diagnosticLine(const vac::combat::ScenarioDiagnostic &diagnostic) {
  return fmt::format("{}:{} {} {}", diagnostic.severity, diagnostic.code,
                     diagnostic.fieldPath, diagnostic.message);
}

std::string
diagnosticLine(const vac::authoring::MapCommandDiagnostic &diagnostic) {
  return fmt::format("{}:{} command={} {} {}", diagnostic.severity,
                     diagnostic.code, diagnostic.commandIndex,
                     diagnostic.fieldPath, diagnostic.message);
}

void textWrapped(std::string_view text) {
  ImGui::TextWrapped("%.*s", static_cast<int>(text.size()), text.data());
}

void textLine(std::string_view text) {
  ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

void bulletLine(std::string_view text) {
  ImGui::BulletText("%.*s", static_cast<int>(text.size()), text.data());
}

struct EditorOptions {
  vac::host::CommonHostOptions common;
  std::filesystem::path mapPath;
  std::filesystem::path commandScriptPath;
  std::filesystem::path movePath;
  std::filesystem::path proxyAnimationPath;
  std::filesystem::path scenarioPath;
  std::filesystem::path characterPath;
  uint32_t frames = 0;
};

EditorOptions parseOptions(int argc, char **argv) {
  vac::host::CommandLine commandLine{argc, argv};
  EditorOptions options;
  options.common = vac::host::parseCommonOptions(commandLine);
  vac::host::rejectUnsupportedCommonOptions(
      options.common,
      {"--frames", "--ticks", "--scene", "--command-script", "--result-file",
       "--headless", "--hidden-window", "--offline", "--seed"});

  options.mapPath =
      projectRoot() / "content/maps/basic_primitive_arena.map.json";
  options.commandScriptPath =
      projectRoot() / "tests/fixtures/commands/wire_arena.commands.json";
  options.movePath = projectRoot() / "content/moves/light_attack.move.json";
  options.proxyAnimationPath =
      projectRoot() / "content/animations/sword_light_proxy.anim.json";
  options.scenarioPath =
      projectRoot() /
      "tests/fixtures/scenarios/sword_light_hits_idle_target.scenario.json";
  options.characterPath =
      projectRoot() / "content/characters/basic_duelists.character.json";

  if (options.common.scene.has_value()) {
    options.mapPath = absoluteFromProject(*options.common.scene);
  }
  if (options.common.commandScript.has_value()) {
    options.commandScriptPath =
        absoluteFromProject(*options.common.commandScript);
  }
  if (options.common.frames.has_value()) {
    options.frames = *options.common.frames;
  }

  if (const std::optional<std::string> value =
          commandLine.consumeValue("--move")) {
    options.movePath = absoluteFromProject(*value);
  }
  if (const std::optional<std::string> value =
          commandLine.consumeValue("--proxy-animation")) {
    options.proxyAnimationPath = absoluteFromProject(*value);
  }
  if (const std::optional<std::string> value =
          commandLine.consumeValue("--scenario")) {
    options.scenarioPath = absoluteFromProject(*value);
  }
  if (const std::optional<std::string> value =
          commandLine.consumeValue("--character")) {
    options.characterPath = absoluteFromProject(*value);
  }

  commandLine.rejectUnknown();
  return options;
}

struct EditorModel {
  std::filesystem::path mapPath;
  std::filesystem::path commandScriptPath;
  std::filesystem::path movePath;
  std::filesystem::path proxyAnimationPath;
  std::filesystem::path scenarioPath;
  std::filesystem::path characterPath;
  std::optional<uint32_t> tickOverride;
  std::optional<uint64_t> seedOverride;

  bool mapLoaded = false;
  vac::content::AuthoringScene mapScene;
  vac::content::CompileResult mapCompile;

  bool commandRan = false;
  vac::authoring::MapCommandResult commandResult;

  bool moveLoaded = false;
  vac::content::MoveAsset moveAsset;
  vac::content::MoveCompileResult moveCompile;

  bool proxyLoaded = false;
  vac::animation::ProxyAnimationAsset proxyAnimation;
  vac::content::ValidationResult proxyValidation;
  vac::animation::ProxyPose proxyPose;
  uint32_t proxyPreviewTick = 0;

  bool characterLoaded = false;
  nlohmann::json characterDocument;
  uint32_t characterCount = 0;

  bool scenarioRan = false;
  vac::combat::ScenarioRunResult scenarioResult;

  bool visualLabBuilt = false;
  vac::visual_lab::VisualLabScene visualLabScene;

  std::vector<std::string> logLines;

  explicit EditorModel(const EditorOptions &options)
      : mapPath(options.mapPath), commandScriptPath(options.commandScriptPath),
        movePath(options.movePath),
        proxyAnimationPath(options.proxyAnimationPath),
        scenarioPath(options.scenarioPath),
        characterPath(options.characterPath),
        tickOverride(options.common.ticks), seedOverride(options.common.seed) {}

  void log(std::string line) { logLines.push_back(std::move(line)); }

  void loadMap() {
    try {
      mapScene = vac::content::loadAuthoringScene(mapPath);
      mapCompile = vac::content::compileRuntimeWorld(mapScene);
      mapLoaded = true;
      log(fmt::format("Loaded map '{}'", displayPath(mapPath)));
    } catch (const std::exception &error) {
      mapLoaded = false;
      mapCompile.validation.diagnostics.clear();
      mapCompile.validation.diagnostics.push_back(
          {"error", "load_map_failed", displayPath(mapPath), "",
           "AuthoringScene", "", error.what()});
      log(fmt::format("Map load failed: {}", error.what()));
    }
  }

  void runCommandScript() {
    try {
      vac::authoring::MapCommandRunOptions options;
      options.workingDirectory = commandScriptPath.parent_path();
      options.defaultPlayTicks = tickOverride;
      commandResult =
          vac::authoring::runMapCommandScript(commandScriptPath, options);
      commandRan = true;
      log(fmt::format("Ran command script '{}'",
                      displayPath(commandScriptPath)));
    } catch (const std::exception &error) {
      commandResult = {};
      commandResult.status = "error";
      commandResult.message = error.what();
      commandRan = true;
      log(fmt::format("Command script failed: {}", error.what()));
    }
  }

  void loadMove() {
    try {
      moveAsset = vac::content::loadMoveAsset(movePath);
      moveCompile = vac::content::compileMoveAsset(moveAsset);
      moveLoaded = true;
      log(fmt::format("Loaded move '{}'", displayPath(movePath)));
    } catch (const std::exception &error) {
      moveLoaded = false;
      moveCompile.validation.diagnostics.clear();
      moveCompile.validation.diagnostics.push_back(
          {"error", "load_move_failed", displayPath(movePath), "", "MoveAsset",
           "", error.what()});
      log(fmt::format("Move load failed: {}", error.what()));
    }
  }

  void loadProxyAnimation() {
    try {
      proxyAnimation = vac::animation::loadProxyAnimation(proxyAnimationPath);
      proxyValidation = vac::animation::validateProxyAnimation(proxyAnimation);
      proxyPreviewTick =
          std::min(proxyPreviewTick, proxyAnimation.durationTicks);
      proxyPose = vac::animation::sampleProxyPoseAtTick(proxyAnimation,
                                                        proxyPreviewTick);
      proxyLoaded = true;
      log(fmt::format("Loaded proxy animation '{}'",
                      displayPath(proxyAnimationPath)));
    } catch (const std::exception &error) {
      proxyLoaded = false;
      proxyValidation.diagnostics.clear();
      proxyValidation.diagnostics.push_back(
          {"error", "load_proxy_animation_failed",
           displayPath(proxyAnimationPath), "", "ProxyAnimation", "",
           error.what()});
      log(fmt::format("Proxy animation load failed: {}", error.what()));
    }
  }

  void sampleProxyAnimation(uint32_t tick) {
    if (!proxyLoaded) {
      return;
    }
    proxyPreviewTick = std::min(tick, proxyAnimation.durationTicks);
    proxyPose =
        vac::animation::sampleProxyPoseAtTick(proxyAnimation, proxyPreviewTick);
  }

  void loadCharacterDefinition() {
    try {
      characterDocument = loadJsonFile(characterPath);
      characterCount = 0;
      if (const auto it = characterDocument.find("characters");
          it != characterDocument.end() && it->is_array()) {
        characterCount = static_cast<uint32_t>(it->size());
      }
      characterLoaded = true;
      log(fmt::format("Loaded character definition '{}'",
                      displayPath(characterPath)));
    } catch (const std::exception &error) {
      characterDocument = nlohmann::json::object();
      characterCount = 0;
      characterLoaded = false;
      log(fmt::format("Character definition load failed: {}", error.what()));
    }
  }

  void runScenario() {
    try {
      vac::combat::CombatScenario scenario =
          vac::combat::loadCombatScenario(scenarioPath);
      vac::combat::ScenarioRunOptions options;
      options.overrideTicks = tickOverride;
      options.overrideSeed = seedOverride;
      scenarioResult =
          vac::combat::runCombatScenario(std::move(scenario), options);
      scenarioRan = true;
      log(fmt::format("Ran scenario '{}'", displayPath(scenarioPath)));
    } catch (const std::exception &error) {
      scenarioResult = {};
      scenarioResult.status = "error";
      scenarioResult.message = error.what();
      scenarioRan = true;
      log(fmt::format("Scenario failed: {}", error.what()));
    }
  }

  void buildVisualLabFromScenario() {
    try {
      visualLabScene =
          vac::visual_lab::buildVisualLabSceneFromScenario(scenarioPath);
      visualLabBuilt = true;
      log("Built visual lab from scenario");
    } catch (const std::exception &error) {
      visualLabScene = {};
      visualLabScene.diagnostics.push_back(error.what());
      visualLabBuilt = true;
      log(fmt::format("Visual lab scenario build failed: {}", error.what()));
    }
  }

  void buildVisualLabFromAssets() {
    try {
      visualLabScene = vac::visual_lab::buildVisualLabScene({
          mapPath,
          movePath,
          proxyAnimationPath,
      });
      visualLabBuilt = true;
      log("Built visual lab from direct assets");
    } catch (const std::exception &error) {
      visualLabScene = {};
      visualLabScene.diagnostics.push_back(error.what());
      visualLabBuilt = true;
      log(fmt::format("Visual lab asset build failed: {}", error.what()));
    }
  }

  void refreshAll() {
    logLines.clear();
    loadMap();
    runCommandScript();
    loadMove();
    loadProxyAnimation();
    loadCharacterDefinition();
    runScenario();
    buildVisualLabFromScenario();
  }

  bool ok() const {
    return mapLoaded && mapCompile.ok() && commandRan && commandResult.ok() &&
           moveLoaded && moveCompile.ok() && proxyLoaded &&
           proxyValidation.ok() && characterLoaded && scenarioRan &&
           scenarioResult.status == "ok" && visualLabBuilt &&
           visualLabScene.diagnostics.empty();
  }

  std::vector<std::string> resultDiagnostics() const {
    std::vector<std::string> diagnostics;
    diagnostics.push_back("editor=true");
    diagnostics.push_back(
        fmt::format("mapLoaded={}", mapLoaded ? "true" : "false"));
    if (mapLoaded) {
      diagnostics.push_back(
          fmt::format("mapObjectCount={}", mapScene.objects.size()));
      diagnostics.push_back(
          fmt::format("mapSpawnPointCount={}", mapScene.spawnPoints.size()));
      diagnostics.push_back(fmt::format("mapRuntimeEntityCount={}",
                                        mapCompile.world.entities.size()));
      diagnostics.push_back(fmt::format("mapRuntimeColliderCount={}",
                                        mapCompile.world.colliders.size()));
    }
    diagnostics.push_back(fmt::format(
        "commandStatus={}", commandRan ? commandResult.status : "not-run"));
    if (commandRan) {
      diagnostics.push_back(
          fmt::format("commandCount={}", commandResult.commandCount));
      diagnostics.push_back(
          fmt::format("commandTicksPlayed={}", commandResult.ticksPlayed));
    }
    diagnostics.push_back(
        fmt::format("moveLoaded={}", moveLoaded ? "true" : "false"));
    if (moveLoaded) {
      diagnostics.push_back(
          fmt::format("movePhaseCount={}", moveAsset.phases.size()));
      diagnostics.push_back(
          fmt::format("moveHitboxCount={}", moveAsset.hitboxTracks.size()));
      diagnostics.push_back(
          fmt::format("moveCancelCount={}", moveAsset.cancels.size()));
    }
    diagnostics.push_back(
        fmt::format("proxyLoaded={}", proxyLoaded ? "true" : "false"));
    if (proxyLoaded) {
      diagnostics.push_back(
          fmt::format("proxySocketCount={}", proxyAnimation.sockets.size()));
    }
    diagnostics.push_back(
        fmt::format("characterLoaded={}", characterLoaded ? "true" : "false"));
    diagnostics.push_back(fmt::format("characterCount={}", characterCount));
    diagnostics.push_back(fmt::format(
        "scenarioStatus={}", scenarioRan ? scenarioResult.status : "not-run"));
    if (scenarioRan) {
      diagnostics.push_back(
          fmt::format("scenarioGoldenCompared={}",
                      scenarioResult.goldenCompared ? "true" : "false"));
      diagnostics.push_back(
          fmt::format("scenarioGoldenMatched={}",
                      scenarioResult.goldenMatched ? "true" : "false"));
      diagnostics.push_back(fmt::format("scenarioTraceEventCount={}",
                                        scenarioResult.trace.events.size()));
      diagnostics.push_back(fmt::format("scenarioFinalStateHash={}",
                                        scenarioResult.trace.finalStateHash));
    }
    diagnostics.push_back(
        fmt::format("visualLabBuilt={}", visualLabBuilt ? "true" : "false"));
    if (visualLabBuilt) {
      diagnostics.push_back(fmt::format("visualLabPrimitiveCount={}",
                                        visualLabScene.summary.primitiveCount));
      diagnostics.push_back(
          fmt::format("visualLabDebugLineVertexCount={}",
                      visualLabScene.summary.debugLineVertexCount));
      diagnostics.push_back(fmt::format("visualLabPlaybackDuration={}",
                                        visualLabScene.playback.durationTicks));
    }
    diagnostics.insert(diagnostics.end(),
                       visualLabScene.resultDiagnostics.begin(),
                       visualLabScene.resultDiagnostics.end());
    return diagnostics;
  }
};

struct PreviewBounds2 {
  float minX = 0.0f;
  float maxX = 0.0f;
  float minZ = 0.0f;
  float maxZ = 0.0f;
  bool valid = false;

  void include(float x, float z) {
    if (!std::isfinite(x) || !std::isfinite(z)) {
      return;
    }
    if (!valid) {
      minX = maxX = x;
      minZ = maxZ = z;
      valid = true;
      return;
    }
    minX = std::min(minX, x);
    maxX = std::max(maxX, x);
    minZ = std::min(minZ, z);
    maxZ = std::max(maxZ, z);
  }

  void includePoint(const glm::vec3 &point) { include(point.x, point.z); }

  void includeWorldBounds(const vac::content::WorldBounds &bounds) {
    include(bounds.min.x, bounds.min.z);
    include(bounds.max.x, bounds.max.z);
  }

  void includeSceneBounds(const vac::Bounds &bounds) {
    if (!bounds.valid) {
      return;
    }
    include(bounds.min.x, bounds.min.z);
    include(bounds.max.x, bounds.max.z);
  }

  void includeRect(const glm::vec3 &center, const glm::vec3 &half) {
    include(center.x - half.x, center.z - half.z);
    include(center.x + half.x, center.z + half.z);
  }

  [[nodiscard]] float width() const { return std::max(maxX - minX, 1.0f); }
  [[nodiscard]] float depth() const { return std::max(maxZ - minZ, 1.0f); }

  void expand(float amount) {
    if (!valid) {
      return;
    }
    minX -= amount;
    maxX += amount;
    minZ -= amount;
    maxZ += amount;
  }
};

struct PreviewTransform2 {
  ImVec2 origin{};
  ImVec2 size{};
  PreviewBounds2 bounds;
  float scale = 1.0f;
  float centerX = 0.0f;
  float centerZ = 0.0f;

  [[nodiscard]] ImVec2 toScreen(float x, float z) const {
    return {
        origin.x + size.x * 0.5f + (x - centerX) * scale,
        origin.y + size.y * 0.5f - (z - centerZ) * scale,
    };
  }

  [[nodiscard]] ImVec2 toScreen(const glm::vec3 &point) const {
    return toScreen(point.x, point.z);
  }
};

glm::vec3 absoluteScale(const glm::vec3 &scale) {
  return {std::abs(scale.x), std::abs(scale.y), std::abs(scale.z)};
}

glm::vec3 authoredHalfSize(const vac::content::AuthoringObject &object) {
  const glm::vec3 scale = absoluteScale(object.transform.scale);
  const glm::vec3 size = object.primitive.size * scale;
  return glm::max(size * 0.5f, glm::vec3{0.12f});
}

glm::vec3 proceduralHalfSize(const vac::ProceduralInstance &instance) {
  const glm::vec3 scale = absoluteScale(instance.transform.scale);
  if (instance.type == "plane") {
    return {
        std::max(instance.size.x * scale.x * 0.5f, 0.12f),
        0.12f,
        std::max(instance.size.y * scale.z * 0.5f, 0.12f),
    };
  }
  return glm::max(instance.size3 * scale * 0.5f, glm::vec3{0.12f});
}

bool roundedPrimitive(std::string_view type) {
  return type == "sphere" || type == "capsule" || type == "cylinder" ||
         type == "actor_root";
}

bool roundedPrimitive(vac::content::PrimitiveKind kind) {
  return kind == vac::content::PrimitiveKind::sphere ||
         kind == vac::content::PrimitiveKind::capsule ||
         kind == vac::content::PrimitiveKind::cylinder;
}

ImU32 previewColor(const glm::vec4 &color, float alphaScale = 1.0f) {
  return ImGui::GetColorU32(ImVec4{
      std::clamp(color.r, 0.0f, 1.0f),
      std::clamp(color.g, 0.0f, 1.0f),
      std::clamp(color.b, 0.0f, 1.0f),
      std::clamp(color.a * alphaScale, 0.0f, 1.0f),
  });
}

ImU32 previewColor(const glm::vec3 &color, float alpha = 1.0f) {
  return ImGui::GetColorU32(ImVec4{
      std::clamp(color.r, 0.0f, 1.0f),
      std::clamp(color.g, 0.0f, 1.0f),
      std::clamp(color.b, 0.0f, 1.0f),
      std::clamp(alpha, 0.0f, 1.0f),
  });
}

ImU32 spawnPreviewColor(std::string_view team) {
  if (team == "blue" || team == "player" || team == "p1") {
    return ImGui::GetColorU32(ImVec4{0.18f, 0.72f, 1.0f, 1.0f});
  }
  if (team == "red" || team == "enemy" || team == "p2") {
    return ImGui::GetColorU32(ImVec4{1.0f, 0.34f, 0.25f, 1.0f});
  }
  return ImGui::GetColorU32(ImVec4{0.95f, 0.80f, 0.28f, 1.0f});
}

void includeDebugLineBounds(PreviewBounds2 &bounds,
                            const vac::SceneDrawData &drawData) {
  for (const vac::SceneVertex &vertex : drawData.lineVertices) {
    bounds.includePoint(vertex.position);
  }
}

PreviewBounds2 buildPreviewBounds(const EditorModel &model) {
  PreviewBounds2 bounds;
  if (model.mapLoaded) {
    bounds.includeWorldBounds(model.mapScene.worldBounds);
    for (const vac::content::AuthoringObject &object : model.mapScene.objects) {
      bounds.includeRect(object.transform.translation,
                         authoredHalfSize(object));
    }
    for (const vac::content::SpawnPoint &spawn : model.mapScene.spawnPoints) {
      bounds.includePoint(spawn.translation);
    }
  }

  if (model.visualLabBuilt) {
    bounds.includeSceneBounds(model.visualLabScene.scene.worldBounds);
    for (const vac::ProceduralInstance &instance :
         model.visualLabScene.scene.procedural) {
      if (instance.worldBounds.valid) {
        bounds.includeSceneBounds(instance.worldBounds);
      } else {
        bounds.includeRect(instance.transform.translation,
                           proceduralHalfSize(instance));
      }
    }
    for (const vac::SceneInstance &instance :
         model.visualLabScene.scene.instances) {
      bounds.includeSceneBounds(instance.worldBounds);
    }
    includeDebugLineBounds(bounds, model.visualLabScene.debugDraw);
  }

  if (!bounds.valid) {
    bounds.include(-10.0f, -10.0f);
    bounds.include(10.0f, 10.0f);
  }

  bounds.expand(
      std::max(std::max(bounds.width(), bounds.depth()) * 0.08f, 1.0f));
  return bounds;
}

PreviewTransform2 makePreviewTransform(const ImVec2 &origin, const ImVec2 &size,
                                       const PreviewBounds2 &bounds) {
  constexpr float padding = 26.0f;
  const float usableWidth = std::max(size.x - padding * 2.0f, 1.0f);
  const float usableHeight = std::max(size.y - padding * 2.0f, 1.0f);

  PreviewTransform2 transform;
  transform.origin = origin;
  transform.size = size;
  transform.bounds = bounds;
  transform.scale =
      std::min(usableWidth / bounds.width(), usableHeight / bounds.depth());
  transform.centerX = (bounds.minX + bounds.maxX) * 0.5f;
  transform.centerZ = (bounds.minZ + bounds.maxZ) * 0.5f;
  return transform;
}

float previewGridStep(const PreviewBounds2 &bounds) {
  const float extent = std::max(bounds.width(), bounds.depth());
  if (extent <= 18.0f) {
    return 1.0f;
  }
  if (extent <= 70.0f) {
    return 5.0f;
  }
  if (extent <= 180.0f) {
    return 10.0f;
  }
  return 25.0f;
}

void drawPreviewGrid(ImDrawList *drawList, const PreviewTransform2 &transform) {
  const ImVec2 bottomRight{transform.origin.x + transform.size.x,
                           transform.origin.y + transform.size.y};
  const ImU32 gridColor =
      ImGui::GetColorU32(ImVec4{0.28f, 0.31f, 0.34f, 0.52f});
  const ImU32 axisColor =
      ImGui::GetColorU32(ImVec4{0.50f, 0.55f, 0.60f, 0.70f});
  const float step = previewGridStep(transform.bounds);

  const float firstX = std::floor(transform.bounds.minX / step) * step;
  for (float x = firstX; x <= transform.bounds.maxX; x += step) {
    const ImVec2 a = transform.toScreen(x, transform.bounds.minZ);
    const ImVec2 b = transform.toScreen(x, transform.bounds.maxZ);
    drawList->AddLine({a.x, transform.origin.y}, {b.x, bottomRight.y},
                      std::abs(x) < 0.001f ? axisColor : gridColor, 1.0f);
  }

  const float firstZ = std::floor(transform.bounds.minZ / step) * step;
  for (float z = firstZ; z <= transform.bounds.maxZ; z += step) {
    const ImVec2 a = transform.toScreen(transform.bounds.minX, z);
    const ImVec2 b = transform.toScreen(transform.bounds.maxX, z);
    drawList->AddLine({transform.origin.x, a.y}, {bottomRight.x, b.y},
                      std::abs(z) < 0.001f ? axisColor : gridColor, 1.0f);
  }
}

void drawPreviewWorldBounds(ImDrawList *drawList,
                            const PreviewTransform2 &transform,
                            const vac::content::WorldBounds &bounds) {
  const ImVec2 min = transform.toScreen(bounds.min.x, bounds.min.z);
  const ImVec2 max = transform.toScreen(bounds.max.x, bounds.max.z);
  drawList->AddRect({std::min(min.x, max.x), std::min(min.y, max.y)},
                    {std::max(min.x, max.x), std::max(min.y, max.y)},
                    ImGui::GetColorU32(ImVec4{0.62f, 0.92f, 0.72f, 1.0f}), 0.0f,
                    0, 2.0f);
}

void drawPreviewBounds(ImDrawList *drawList, const PreviewTransform2 &transform,
                       const vac::Bounds &bounds, ImU32 color,
                       float thickness = 1.0f) {
  if (!bounds.valid) {
    return;
  }
  const ImVec2 min = transform.toScreen(bounds.min.x, bounds.min.z);
  const ImVec2 max = transform.toScreen(bounds.max.x, bounds.max.z);
  drawList->AddRect({std::min(min.x, max.x), std::min(min.y, max.y)},
                    {std::max(min.x, max.x), std::max(min.y, max.y)}, color,
                    0.0f, 0, thickness);
}

void drawPreviewPrimitive(ImDrawList *drawList,
                          const PreviewTransform2 &transform,
                          const glm::vec3 &center, const glm::vec3 &half,
                          bool rounded, ImU32 fillColor, ImU32 outlineColor) {
  const ImVec2 centerScreen = transform.toScreen(center);
  if (rounded) {
    const float radius =
        std::max(std::max(half.x, half.z) * transform.scale, 4.0f);
    drawList->AddCircleFilled(centerScreen, radius, fillColor, 32);
    drawList->AddCircle(centerScreen, radius, outlineColor, 32, 1.5f);
    return;
  }

  const ImVec2 min = transform.toScreen(center.x - half.x, center.z - half.z);
  const ImVec2 max = transform.toScreen(center.x + half.x, center.z + half.z);
  drawList->AddRectFilled({std::min(min.x, max.x), std::min(min.y, max.y)},
                          {std::max(min.x, max.x), std::max(min.y, max.y)},
                          fillColor);
  drawList->AddRect({std::min(min.x, max.x), std::min(min.y, max.y)},
                    {std::max(min.x, max.x), std::max(min.y, max.y)},
                    outlineColor, 0.0f, 0, 1.5f);
}

void drawAuthoringObjects(ImDrawList *drawList,
                          const PreviewTransform2 &transform,
                          const vac::content::AuthoringScene &scene) {
  const bool drawLabels = scene.objects.size() <= 24;
  const ImU32 outlineColor =
      ImGui::GetColorU32(ImVec4{0.96f, 0.98f, 1.0f, 0.95f});
  const ImU32 labelColor =
      ImGui::GetColorU32(ImVec4{0.94f, 0.96f, 0.98f, 0.92f});

  for (const vac::content::AuthoringObject &object : scene.objects) {
    const glm::vec3 half = authoredHalfSize(object);
    drawPreviewPrimitive(drawList, transform, object.transform.translation,
                         half, roundedPrimitive(object.primitive.kind),
                         previewColor(object.material.baseColor, 0.58f),
                         outlineColor);
    if (drawLabels) {
      const ImVec2 labelPos = transform.toScreen(object.transform.translation);
      drawList->AddText({labelPos.x + 5.0f, labelPos.y + 5.0f}, labelColor,
                        object.id.value.c_str());
    }
  }
}

void drawSpawnMarker(ImDrawList *drawList, const PreviewTransform2 &transform,
                     const vac::content::SpawnPoint &spawn) {
  constexpr float pi = 3.14159265358979323846f;
  const ImU32 color =
      spawn.enabled ? spawnPreviewColor(spawn.team)
                    : ImGui::GetColorU32(ImVec4{0.58f, 0.60f, 0.62f, 0.65f});
  const ImVec2 center = transform.toScreen(spawn.translation);
  const float yaw = spawn.yawDegrees * pi / 180.0f;
  const ImVec2 forward{std::sin(yaw), -std::cos(yaw)};
  const ImVec2 right{forward.y, -forward.x};
  const float radius = 10.0f;
  const ImVec2 tip{center.x + forward.x * radius,
                   center.y + forward.y * radius};
  const ImVec2 left{
      center.x - forward.x * radius * 0.55f + right.x * radius * 0.55f,
      center.y - forward.y * radius * 0.55f + right.y * radius * 0.55f};
  const ImVec2 rightPoint{
      center.x - forward.x * radius * 0.55f - right.x * radius * 0.55f,
      center.y - forward.y * radius * 0.55f - right.y * radius * 0.55f};

  drawList->AddTriangleFilled(tip, left, rightPoint, color);
  drawList->AddTriangle(tip, left, rightPoint,
                        ImGui::GetColorU32(ImVec4{0.04f, 0.05f, 0.06f, 1.0f}),
                        1.0f);
}

void drawAuthoringSpawns(ImDrawList *drawList,
                         const PreviewTransform2 &transform,
                         const vac::content::AuthoringScene &scene) {
  for (const vac::content::SpawnPoint &spawn : scene.spawnPoints) {
    drawSpawnMarker(drawList, transform, spawn);
  }
}

void drawVisualLabProcedural(ImDrawList *drawList,
                             const PreviewTransform2 &transform,
                             const vac::visual_lab::VisualLabScene &scene) {
  const ImU32 outlineColor =
      ImGui::GetColorU32(ImVec4{0.18f, 0.20f, 0.23f, 0.95f});
  for (const vac::ProceduralInstance &instance : scene.scene.procedural) {
    if (instance.worldBounds.valid) {
      drawPreviewBounds(drawList, transform, instance.worldBounds,
                        previewColor(instance.baseColor, 0.88f), 1.5f);
    }
    drawPreviewPrimitive(drawList, transform, instance.transform.translation,
                         proceduralHalfSize(instance),
                         roundedPrimitive(instance.type),
                         previewColor(instance.baseColor, 0.30f), outlineColor);
  }

  const ImU32 instanceColor =
      ImGui::GetColorU32(ImVec4{0.78f, 0.56f, 1.0f, 0.90f});
  for (const vac::SceneInstance &instance : scene.scene.instances) {
    drawPreviewBounds(drawList, transform, instance.worldBounds, instanceColor,
                      1.4f);
  }
}

void drawVisualLabDebugLines(ImDrawList *drawList,
                             const PreviewTransform2 &transform,
                             const vac::SceneDrawData &drawData) {
  const std::vector<vac::SceneVertex> &lines = drawData.lineVertices;
  for (size_t i = 1; i < lines.size(); i += 2) {
    const vac::SceneVertex &a = lines[i - 1];
    const vac::SceneVertex &b = lines[i];
    drawList->AddLine(transform.toScreen(a.position),
                      transform.toScreen(b.position),
                      previewColor(a.color, 0.92f), 1.4f);
  }
}

void drawPreviewLegend(ImDrawList *drawList, const ImVec2 &origin,
                       const EditorModel &model) {
  const std::string summary = fmt::format(
      "Map Preview  objects={} spawns={} lab_primitives={} debug_lines={}",
      model.mapLoaded ? model.mapScene.objects.size() : 0,
      model.mapLoaded ? model.mapScene.spawnPoints.size() : 0,
      model.visualLabBuilt ? model.visualLabScene.summary.primitiveCount : 0,
      model.visualLabBuilt ? model.visualLabScene.summary.debugLineVertexCount
                           : 0);
  drawList->AddText({origin.x + 12.0f, origin.y + 10.0f},
                    ImGui::GetColorU32(ImVec4{0.96f, 0.97f, 0.98f, 1.0f}),
                    summary.c_str());
  drawList->AddText({origin.x + 12.0f, origin.y + 30.0f},
                    ImGui::GetColorU32(ImVec4{0.70f, 0.76f, 0.82f, 1.0f}),
                    model.visualLabBuilt ? "Base map with visual-lab overlay"
                                         : "Base map only");
}

void drawMapPreviewPanel(EditorModel &model) {
  ImGui::SetNextWindowPos({448.0f, 16.0f}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({740.0f, 560.0f}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Map Preview");
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const ImVec2 canvasSize{
      std::max(available.x, 360.0f),
      std::clamp(available.y > 160.0f ? available.y : 420.0f, 320.0f, 560.0f),
  };
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  const ImVec2 bottomRight{origin.x + canvasSize.x, origin.y + canvasSize.y};
  drawList->AddRectFilled(
      origin, bottomRight,
      ImGui::GetColorU32(ImVec4{0.075f, 0.085f, 0.095f, 1.0f}));
  drawList->AddRect(origin, bottomRight,
                    ImGui::GetColorU32(ImVec4{0.32f, 0.35f, 0.38f, 1.0f}), 0.0f,
                    0, 1.0f);

  const PreviewBounds2 bounds = buildPreviewBounds(model);
  const PreviewTransform2 transform =
      makePreviewTransform(origin, canvasSize, bounds);
  drawList->PushClipRect(origin, bottomRight, true);
  drawPreviewGrid(drawList, transform);

  if (model.mapLoaded) {
    drawPreviewWorldBounds(drawList, transform, model.mapScene.worldBounds);
    drawAuthoringObjects(drawList, transform, model.mapScene);
    drawAuthoringSpawns(drawList, transform, model.mapScene);
  }

  if (model.visualLabBuilt) {
    drawVisualLabProcedural(drawList, transform, model.visualLabScene);
    drawVisualLabDebugLines(drawList, transform,
                            model.visualLabScene.debugDraw);
  }

  if (!model.mapLoaded && !model.visualLabBuilt) {
    drawList->AddText({origin.x + 12.0f, origin.y + 12.0f},
                      ImGui::GetColorU32(ImVec4{0.86f, 0.90f, 0.94f, 1.0f}),
                      "Reload Map or Build Direct Visual Lab to populate this "
                      "preview.");
  } else {
    drawPreviewLegend(drawList, origin, model);
  }

  drawList->PopClipRect();
  ImGui::InvisibleButton("map_preview_canvas", canvasSize);
  ImGui::End();
}

void drawDiagnostics(
    const std::vector<vac::content::ContentDiagnostic> &diagnostics) {
  if (diagnostics.empty()) {
    textLine("No diagnostics.");
    return;
  }
  for (const auto &diagnostic : diagnostics) {
    bulletLine(diagnosticLine(diagnostic));
  }
}

void drawOverview(EditorModel &model) {
  ImGui::Begin("Editor Overview");
  ImGui::Text("Game Editor Bootstrap");
  ImGui::Separator();
  ImGui::Text("Project");
  textWrapped(displayPath(projectRoot()));

  if (ImGui::Button("Refresh All")) {
    model.refreshAll();
  }
  ImGui::SameLine();
  if (ImGui::Button("Run Scenario")) {
    model.runScenario();
    model.buildVisualLabFromScenario();
  }
  ImGui::SameLine();
  if (ImGui::Button("Run Map Script")) {
    model.runCommandScript();
  }

  ImGui::Separator();
  ImGui::Text("Current Inputs");
  bulletLine(fmt::format("Map: {}", displayPath(model.mapPath)));
  bulletLine(
      fmt::format("Command script: {}", displayPath(model.commandScriptPath)));
  bulletLine(fmt::format("Move: {}", displayPath(model.movePath)));
  bulletLine(fmt::format("Proxy animation: {}",
                         displayPath(model.proxyAnimationPath)));
  bulletLine(
      fmt::format("Character file: {}", displayPath(model.characterPath)));
  bulletLine(fmt::format("Scenario: {}", displayPath(model.scenarioPath)));

  ImGui::Separator();
  ImGui::Text("Status");
  bulletLine(fmt::format("Map: {}", model.mapLoaded && model.mapCompile.ok()
                                        ? "ok"
                                        : "needs attention"));
  bulletLine(fmt::format("Move: {}", model.moveLoaded && model.moveCompile.ok()
                                         ? "ok"
                                         : "needs attention"));
  bulletLine(fmt::format("Scenario: {}", model.scenarioRan
                                             ? model.scenarioResult.status
                                             : "not run"));
  bulletLine(fmt::format("Visual lab: {}",
                         model.visualLabBuilt &&
                                 model.visualLabScene.diagnostics.empty()
                             ? "ok"
                             : "needs attention"));
  ImGui::End();
}

void drawMapPanel(EditorModel &model) {
  ImGui::Begin("Map Authoring");
  if (ImGui::Button("Reload Map")) {
    model.loadMap();
  }
  ImGui::SameLine();
  if (ImGui::Button("Build Direct Visual Lab")) {
    model.buildVisualLabFromAssets();
  }

  ImGui::Separator();
  textWrapped(displayPath(model.mapPath));
  if (!model.mapLoaded) {
    drawDiagnostics(model.mapCompile.validation.diagnostics);
    ImGui::End();
    return;
  }

  ImGui::Text("Logical ID: %s", model.mapScene.logicalId.c_str());
  ImGui::Text("Objects: %d", static_cast<int>(model.mapScene.objects.size()));
  ImGui::Text("Spawn points: %d",
              static_cast<int>(model.mapScene.spawnPoints.size()));
  ImGui::Text("Runtime entities: %d",
              static_cast<int>(model.mapCompile.world.entities.size()));
  ImGui::Text("Runtime colliders: %d",
              static_cast<int>(model.mapCompile.world.colliders.size()));

  if (ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::BeginTable("map_objects", 5,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("ID");
      ImGui::TableSetupColumn("Name");
      ImGui::TableSetupColumn("Primitive");
      ImGui::TableSetupColumn("Collider");
      ImGui::TableSetupColumn("Base Color");
      ImGui::TableHeadersRow();
      for (const vac::content::AuthoringObject &object :
           model.mapScene.objects) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        textLine(object.id.value);
        ImGui::TableSetColumnIndex(1);
        textLine(object.name);
        ImGui::TableSetColumnIndex(2);
        textLine(vac::content::toString(object.primitive.kind));
        ImGui::TableSetColumnIndex(3);
        textLine(object.collider.has_value()
                     ? vac::content::toString(object.collider->kind)
                     : "none");
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.2f %.2f %.2f %.2f", object.material.baseColor.r,
                    object.material.baseColor.g, object.material.baseColor.b,
                    object.material.baseColor.a);
      }
      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Spawn Points", ImGuiTreeNodeFlags_DefaultOpen)) {
    for (const vac::content::SpawnPoint &spawn : model.mapScene.spawnPoints) {
      bulletLine(fmt::format("{} team={} yaw={:.1f}", spawn.id, spawn.team,
                             spawn.yawDegrees));
    }
  }

  if (ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
    drawDiagnostics(model.mapCompile.validation.diagnostics);
  }
  ImGui::End();
}

void drawCommandPanel(EditorModel &model) {
  ImGui::Begin("Map Command Script");
  if (ImGui::Button("Run Command Script")) {
    model.runCommandScript();
  }
  ImGui::Separator();
  textWrapped(displayPath(model.commandScriptPath));
  if (!model.commandRan) {
    textLine("Not run yet.");
    ImGui::End();
    return;
  }

  ImGui::Text("Status: %s", model.commandResult.status.c_str());
  textWrapped(model.commandResult.message);
  ImGui::Text("Commands: %u", model.commandResult.commandCount);
  ImGui::Text("Created objects: %d",
              static_cast<int>(model.commandResult.createdObjectIds.size()));
  ImGui::Text("Saved path: %s",
              displayPath(model.commandResult.savedPath.value_or({})).c_str());
  ImGui::Text("Entities: %u", model.commandResult.entityCount);
  ImGui::Text("Colliders: %u", model.commandResult.colliderCount);
  ImGui::Text("Spawn points: %u", model.commandResult.spawnPointCount);
  ImGui::Text("Ticks played: %u", model.commandResult.ticksPlayed);
  ImGui::Text("State hash: %llu",
              static_cast<unsigned long long>(model.commandResult.stateHash));

  if (ImGui::CollapsingHeader("Created IDs")) {
    for (const std::string &id : model.commandResult.createdObjectIds) {
      bulletLine(id);
    }
  }
  if (ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (model.commandResult.diagnostics.empty()) {
      textLine("No diagnostics.");
    } else {
      for (const auto &diagnostic : model.commandResult.diagnostics) {
        bulletLine(diagnosticLine(diagnostic));
      }
    }
  }
  ImGui::End();
}

void drawMovePanel(EditorModel &model) {
  ImGui::Begin("Move Timeline");
  if (ImGui::Button("Reload Move")) {
    model.loadMove();
  }
  ImGui::Separator();
  textWrapped(displayPath(model.movePath));
  if (!model.moveLoaded) {
    drawDiagnostics(model.moveCompile.validation.diagnostics);
    ImGui::End();
    return;
  }

  ImGui::Text("Move: %s", model.moveAsset.logicalId.c_str());
  ImGui::Text("Duration: %u ticks", model.moveAsset.durationTicks);
  ImGui::Text("Command: %s", model.moveAsset.input.command.c_str());
  ImGui::Text("Phases: %d", static_cast<int>(model.moveAsset.phases.size()));
  ImGui::Text("Hitboxes: %d",
              static_cast<int>(model.moveAsset.hitboxTracks.size()));
  ImGui::Text("Cancels: %d", static_cast<int>(model.moveAsset.cancels.size()));

  if (ImGui::CollapsingHeader("Phases", ImGuiTreeNodeFlags_DefaultOpen)) {
    for (const auto &phase : model.moveAsset.phases) {
      bulletLine(fmt::format("{} [{}..{})", phase.id, phase.range.begin,
                             phase.range.end));
    }
  }

  if (ImGui::CollapsingHeader("Hitboxes", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::BeginTable("hitboxes", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("ID");
      ImGui::TableSetupColumn("Range");
      ImGui::TableSetupColumn("Shape");
      ImGui::TableSetupColumn("Socket");
      ImGui::TableSetupColumn("Damage");
      ImGui::TableSetupColumn("Hitstop");
      ImGui::TableSetupColumn("Reaction");
      ImGui::TableHeadersRow();
      for (const auto &hitbox : model.moveAsset.hitboxTracks) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        textLine(hitbox.id);
        ImGui::TableSetColumnIndex(1);
        textLine(fmt::format("{}..{}", hitbox.range.begin, hitbox.range.end));
        ImGui::TableSetColumnIndex(2);
        textLine(hitbox.shape);
        ImGui::TableSetColumnIndex(3);
        textLine(hitbox.socket);
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%u", hitbox.damage);
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%u", hitbox.hitstopTicks);
        ImGui::TableSetColumnIndex(6);
        textLine(hitbox.reactionMove.empty() ? "none" : hitbox.reactionMove);
      }
      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Cancels", ImGuiTreeNodeFlags_DefaultOpen)) {
    for (const auto &cancel : model.moveAsset.cancels) {
      bulletLine(fmt::format("{} [{}..{}) condition={} destinations={}",
                             cancel.id, cancel.range.begin, cancel.range.end,
                             cancel.condition, cancel.destinations.size()));
    }
  }

  if (ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
    drawDiagnostics(model.moveCompile.validation.diagnostics);
  }
  ImGui::End();
}

void drawProxyAnimationPanel(EditorModel &model) {
  ImGui::Begin("Proxy Animation");
  if (ImGui::Button("Reload Animation")) {
    model.loadProxyAnimation();
  }
  ImGui::Separator();
  textWrapped(displayPath(model.proxyAnimationPath));
  if (!model.proxyLoaded) {
    drawDiagnostics(model.proxyValidation.diagnostics);
    ImGui::End();
    return;
  }

  ImGui::Text("Animation: %s", model.proxyAnimation.logicalId.c_str());
  ImGui::Text("Duration: %u ticks", model.proxyAnimation.durationTicks);
  ImGui::Text("Root keys: %d",
              static_cast<int>(model.proxyAnimation.rootMotion.size()));
  ImGui::Text("Socket tracks: %d",
              static_cast<int>(model.proxyAnimation.sockets.size()));

  int tick = static_cast<int>(model.proxyPreviewTick);
  const int maxTick = static_cast<int>(
      std::min<uint32_t>(model.proxyAnimation.durationTicks, 600));
  if (ImGui::SliderInt("Preview Tick", &tick, 0, maxTick)) {
    model.sampleProxyAnimation(static_cast<uint32_t>(std::max(0, tick)));
  }

  ImGui::Text(
      "Root: %.2f %.2f %.2f yaw=%.1f", model.proxyPose.rootTranslation.x,
      model.proxyPose.rootTranslation.y, model.proxyPose.rootTranslation.z,
      model.proxyPose.rootYawDegrees);

  if (ImGui::CollapsingHeader("Sockets", ImGuiTreeNodeFlags_DefaultOpen)) {
    for (const auto &socket : model.proxyPose.sockets) {
      bulletLine(fmt::format("{} world=({:.2f}, {:.2f}, {:.2f})", socket.name,
                             socket.worldTranslation.x,
                             socket.worldTranslation.y,
                             socket.worldTranslation.z));
    }
  }

  if (ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
    drawDiagnostics(model.proxyValidation.diagnostics);
  }
  ImGui::End();
}

void drawCharacterPanel(EditorModel &model) {
  ImGui::Begin("Character Defaults");
  if (ImGui::Button("Reload Character File")) {
    model.loadCharacterDefinition();
  }
  ImGui::Separator();
  textWrapped(displayPath(model.characterPath));
  if (!model.characterLoaded) {
    textLine("Character file failed to load.");
    ImGui::End();
    return;
  }

  ImGui::Text("Characters: %u", model.characterCount);
  const auto it = model.characterDocument.find("characters");
  if (it != model.characterDocument.end() && it->is_array()) {
    for (const auto &character : *it) {
      const std::string id = character.value("id", "");
      const std::string label = character.value("label", "");
      if (ImGui::TreeNode(id.c_str(), "%s", id.c_str())) {
        textWrapped(label);
        if (const auto healthIt = character.find("defaultHealth");
            healthIt != character.end() && healthIt->is_object()) {
          ImGui::Text("Health: current=%u max=%u",
                      healthIt->value("current", 0u),
                      healthIt->value("max", 0u));
        }
        if (const auto movesIt = character.find("defaultMoves");
            movesIt != character.end() && movesIt->is_array()) {
          ImGui::Text("Default moves:");
          for (const auto &move : *movesIt) {
            if (move.is_string()) {
              bulletLine(move.get<std::string>());
            }
          }
        }
        ImGui::TreePop();
      }
    }
  }
  ImGui::End();
}

void drawScenarioPanel(EditorModel &model) {
  ImGui::Begin("Scenario Runner");
  if (ImGui::Button("Run Scenario")) {
    model.runScenario();
  }
  ImGui::SameLine();
  if (ImGui::Button("Build Scenario Lab")) {
    model.buildVisualLabFromScenario();
  }
  ImGui::Separator();
  textWrapped(displayPath(model.scenarioPath));
  if (!model.scenarioRan) {
    textLine("Not run yet.");
    ImGui::End();
    return;
  }

  ImGui::Text("Status: %s", model.scenarioResult.status.c_str());
  textWrapped(model.scenarioResult.message);
  ImGui::Text("Scenario: %s", model.scenarioResult.trace.scenarioId.c_str());
  ImGui::Text("Ticks run: %u", model.scenarioResult.trace.ticksRun);
  ImGui::Text("Final hash: %s",
              model.scenarioResult.trace.finalStateHash.c_str());
  ImGui::Text("Golden compared: %s",
              model.scenarioResult.goldenCompared ? "true" : "false");
  ImGui::Text("Golden matched: %s",
              model.scenarioResult.goldenMatched ? "true" : "false");
  ImGui::Text("Events: %d",
              static_cast<int>(model.scenarioResult.trace.events.size()));

  if (ImGui::CollapsingHeader("Trace Events", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::BeginTable("trace_events", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Seq");
      ImGui::TableSetupColumn("Tick");
      ImGui::TableSetupColumn("Kind");
      ImGui::TableSetupColumn("Actor");
      ImGui::TableSetupColumn("Target");
      ImGui::TableSetupColumn("Move");
      ImGui::TableSetupColumn("Label");
      ImGui::TableHeadersRow();
      for (const auto &event : model.scenarioResult.trace.events) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%u", event.sequence);
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%u", event.tick);
        ImGui::TableSetColumnIndex(2);
        textLine(event.kind);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%u", event.actorId);
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%u", event.targetId);
        ImGui::TableSetColumnIndex(5);
        textLine(event.move);
        ImGui::TableSetColumnIndex(6);
        textLine(event.label);
      }
      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (model.scenarioResult.diagnostics.empty()) {
      textLine("No diagnostics.");
    } else {
      for (const auto &diagnostic : model.scenarioResult.diagnostics) {
        bulletLine(diagnosticLine(diagnostic));
      }
    }
  }
  ImGui::End();
}

void drawVisualLabPanel(EditorModel &model) {
  ImGui::Begin("Visual Lab");
  if (ImGui::Button("Build From Scenario")) {
    model.buildVisualLabFromScenario();
  }
  ImGui::SameLine();
  if (ImGui::Button("Build From Assets")) {
    model.buildVisualLabFromAssets();
  }
  ImGui::Separator();
  if (!model.visualLabBuilt) {
    textLine("Not built yet.");
    ImGui::End();
    return;
  }

  const vac::visual_lab::VisualLabSummary &summary =
      model.visualLabScene.summary;
  ImGui::Text("Primitives: %u", summary.primitiveCount);
  ImGui::Text("Colliders: %u", summary.colliderCount);
  ImGui::Text("Spawn points: %u", summary.spawnPointCount);
  ImGui::Text("Actor roots: %u", summary.actorRootCount);
  ImGui::Text("Move phase windows: %u", summary.movePhaseWindowCount);
  ImGui::Text("Hitboxes: %u", summary.hitboxVolumeCount);
  ImGui::Text("Hurtboxes: %u", summary.hurtboxVolumeCount);
  ImGui::Text("Proxy sockets: %u", summary.proxySocketMarkerCount);
  ImGui::Text("Root path segments: %u", summary.rootMotionPathSegmentCount);
  ImGui::Text("Debug line vertices: %u", summary.debugLineVertexCount);

  vac::visual_lab::VisualLabPlaybackState &playback =
      model.visualLabScene.playback;
  ImGui::Separator();
  ImGui::Text("Playback tick: %u / %u", playback.currentTick,
              playback.durationTicks);
  if (ImGui::Button(playback.paused ? "Play" : "Pause")) {
    playback = vac::visual_lab::setPlaybackPaused(playback, !playback.paused);
  }
  ImGui::SameLine();
  if (ImGui::Button("Step")) {
    playback = vac::visual_lab::stepPlayback(playback);
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset")) {
    playback = vac::visual_lab::resetPlayback(playback);
  }
  int tick = static_cast<int>(playback.currentTick);
  const int maxTick = static_cast<int>(playback.durationTicks);
  if (ImGui::SliderInt("Seek", &tick, 0, std::max(0, maxTick))) {
    playback = vac::visual_lab::seekPlayback(
        playback, static_cast<uint32_t>(std::max(0, tick)));
  }

  if (ImGui::CollapsingHeader("Scenario Evidence",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    const auto &evidence = model.visualLabScene.scenarioEvidence;
    ImGui::Text("Events: %u", evidence.eventCount);
    ImGui::Text("Hit events: %u", evidence.hitEventCount);
    ImGui::Text("Blocked hits: %u", evidence.blockedHitEventCount);
    ImGui::Text("Total damage: %u", evidence.totalDamage);
    ImGui::Text("Lowest remaining health: %u", evidence.lowestRemainingHealth);
    for (const std::string &move : evidence.reactionMoves) {
      bulletLine(fmt::format("Reaction: {}", move));
    }
  }

  if (ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (model.visualLabScene.diagnostics.empty() &&
        model.visualLabScene.resultDiagnostics.empty()) {
      textLine("No diagnostics.");
    }
    for (const std::string &diagnostic : model.visualLabScene.diagnostics) {
      bulletLine(diagnostic);
    }
    for (const std::string &diagnostic :
         model.visualLabScene.resultDiagnostics) {
      bulletLine(diagnostic);
    }
  }
  ImGui::End();
}

void drawLogPanel(const EditorModel &model) {
  ImGui::Begin("Editor Log");
  for (const std::string &line : model.logLines) {
    bulletLine(line);
  }
  ImGui::End();
}

void drawEditorUi(EditorModel &model) {
  drawOverview(model);
  drawMapPanel(model);
  drawMapPreviewPanel(model);
  drawCommandPanel(model);
  drawMovePanel(model);
  drawProxyAnimationPanel(model);
  drawCharacterPanel(model);
  drawScenarioPanel(model);
  drawVisualLabPanel(model);
  drawLogPanel(model);
}

void checkVk(VkResult result) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(fmt::format("Vulkan call failed with VkResult {}",
                                         static_cast<int>(result)));
  }
}

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsPresent;
};

struct SwapchainSupport {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

class ImGuiVulkanShell {
public:
  explicit ImGuiVulkanShell(const EditorOptions &options)
      : m_options(options) {}

  ~ImGuiVulkanShell() { cleanup(); }

  void run(EditorModel &model) {
    initWindow();
    initVulkan();
    initImGui();
    mainLoop(model);
    vkDeviceWaitIdle(m_device);
  }

private:
  EditorOptions m_options;
  GLFWwindow *m_window = nullptr;
  VkInstance m_instance = VK_NULL_HANDLE;
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
  VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
  VkQueue m_queue = VK_NULL_HANDLE;
  uint32_t m_queueFamily = 0;
  VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
  VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
  VkExtent2D m_swapchainExtent{};
  uint32_t m_minImageCount = 2;
  std::vector<VkImage> m_swapchainImages;
  std::vector<VkImageView> m_swapchainImageViews;
  VkRenderPass m_renderPass = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> m_framebuffers;
  VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
  VkCommandPool m_commandPool = VK_NULL_HANDLE;
  VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
  VkSemaphore m_imageAvailable = VK_NULL_HANDLE;
  VkSemaphore m_renderFinished = VK_NULL_HANDLE;
  VkFence m_inFlight = VK_NULL_HANDLE;
  bool m_imguiInitialized = false;
  bool m_framebufferResized = false;

  static void framebufferResizeCallback(GLFWwindow *window, int, int) {
    auto *shell =
        static_cast<ImGuiVulkanShell *>(glfwGetWindowUserPointer(window));
    if (shell) {
      shell->m_framebufferResized = true;
    }
  }

  void initWindow() {
    if (glfwInit() != GLFW_TRUE) {
      throw std::runtime_error("glfwInit failed");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    if (m_options.common.hiddenWindow) {
      glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }
    m_window = glfwCreateWindow(kEditorWidth, kEditorHeight,
                                "Vulkan Action Client - Game Editor", nullptr,
                                nullptr);
    if (!m_window) {
      throw std::runtime_error("glfwCreateWindow failed");
    }
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
  }

  void createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Action Client Game Editor";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Vulkan Action Client";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t extensionCount = 0;
    const char **extensions =
        glfwGetRequiredInstanceExtensions(&extensionCount);
    if (extensions == nullptr || extensionCount == 0) {
      throw std::runtime_error(
          "GLFW did not provide Vulkan instance extensions");
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;
    checkVk(vkCreateInstance(&createInfo, nullptr, &m_instance));
  }

  void createSurface() {
    checkVk(glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface));
  }

  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
      VkBool32 presentSupported = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface,
                                           &presentSupported);
      if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
          presentSupported == VK_TRUE) {
        indices.graphicsPresent = i;
        return indices;
      }
    }
    return indices;
  }

  bool deviceSupportsSwapchain(VkPhysicalDevice device) const {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                         nullptr);
    std::vector<VkExtensionProperties> available(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                         available.data());

    std::set<std::string> required{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    for (const VkExtensionProperties &extension : available) {
      required.erase(extension.extensionName);
    }
    return required.empty();
  }

  SwapchainSupport querySwapchainSupport(VkPhysicalDevice device) const {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface,
                                              &support.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount,
                                         nullptr);
    support.formats.resize(formatCount);
    if (formatCount > 0) {
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount,
                                           support.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface,
                                              &presentModeCount, nullptr);
    support.presentModes.resize(presentModeCount);
    if (presentModeCount > 0) {
      vkGetPhysicalDeviceSurfacePresentModesKHR(
          device, m_surface, &presentModeCount, support.presentModes.data());
    }
    return support;
  }

  void pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
      throw std::runtime_error("No Vulkan physical devices found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    for (VkPhysicalDevice device : devices) {
      const QueueFamilyIndices families = findQueueFamilies(device);
      if (!families.graphicsPresent.has_value() ||
          !deviceSupportsSwapchain(device)) {
        continue;
      }
      const SwapchainSupport support = querySwapchainSupport(device);
      if (support.formats.empty() || support.presentModes.empty()) {
        continue;
      }
      m_physicalDevice = device;
      m_queueFamily = *families.graphicsPresent;
      return;
    }
    throw std::runtime_error(
        "No suitable Vulkan physical device found for editor");
  }

  void createDevice() {
    constexpr float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const std::array<const char *, 1> extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.pEnabledFeatures = &features;

    checkVk(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));
    vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
  }

  VkSurfaceFormatKHR
  chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats) const {
    for (const VkSurfaceFormatKHR &format : formats) {
      if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
          format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return format;
      }
    }
    return formats.front();
  }

  VkPresentModeKHR
  choosePresentMode(const std::vector<VkPresentModeKHR> &modes) const {
    for (VkPresentModeKHR mode : modes) {
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return mode;
      }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities) const {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    VkExtent2D extent{static_cast<uint32_t>(std::max(1, width)),
                      static_cast<uint32_t>(std::max(1, height))};
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height =
        std::clamp(extent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
    return extent;
  }

  void createSwapchain() {
    const SwapchainSupport support = querySwapchainSupport(m_physicalDevice);
    const VkSurfaceFormatKHR surfaceFormat =
        chooseSurfaceFormat(support.formats);
    const VkPresentModeKHR presentMode =
        choosePresentMode(support.presentModes);
    const VkExtent2D extent = chooseExtent(support.capabilities);

    uint32_t imageCount = std::max(2u, support.capabilities.minImageCount + 1u);
    if (support.capabilities.maxImageCount > 0 &&
        imageCount > support.capabilities.maxImageCount) {
      imageCount = support.capabilities.maxImageCount;
    }
    m_minImageCount = std::max(2u, support.capabilities.minImageCount);

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    checkVk(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain));
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount,
                            m_swapchainImages.data());
    m_swapchainFormat = surfaceFormat.format;
    m_swapchainExtent = extent;
  }

  void createImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImages.size(); ++i) {
      VkImageViewCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      createInfo.image = m_swapchainImages[i];
      createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format = m_swapchainFormat;
      createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;
      checkVk(vkCreateImageView(m_device, &createInfo, nullptr,
                                &m_swapchainImageViews[i]));
    }
  }

  void createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;
    checkVk(vkCreateRenderPass(m_device, &createInfo, nullptr, &m_renderPass));
  }

  void createFramebuffers() {
    m_framebuffers.resize(m_swapchainImageViews.size());
    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i) {
      const std::array<VkImageView, 1> attachments{m_swapchainImageViews[i]};
      VkFramebufferCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      createInfo.renderPass = m_renderPass;
      createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      createInfo.pAttachments = attachments.data();
      createInfo.width = m_swapchainExtent.width;
      createInfo.height = m_swapchainExtent.height;
      createInfo.layers = 1;
      checkVk(vkCreateFramebuffer(m_device, &createInfo, nullptr,
                                  &m_framebuffers[i]));
    }
  }

  void createDescriptorPool() {
    const std::array<VkDescriptorPoolSize, 11> poolSizes{{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    }};

    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    createInfo.maxSets = 1000 * static_cast<uint32_t>(poolSizes.size());
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.data();
    checkVk(vkCreateDescriptorPool(m_device, &createInfo, nullptr,
                                   &m_descriptorPool));
  }

  void createCommandResources() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_queueFamily;
    checkVk(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool));

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = m_commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    checkVk(
        vkAllocateCommandBuffers(m_device, &allocateInfo, &m_commandBuffer));
  }

  void createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    checkVk(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr,
                              &m_imageAvailable));
    checkVk(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr,
                              &m_renderFinished));

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    checkVk(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlight));
  }

  void initVulkan() {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createDevice();
    createDescriptorPool();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createFramebuffers();
    createCommandResources();
    createSyncObjects();
  }

  void initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(m_window, true);
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_0;
    initInfo.Instance = m_instance;
    initInfo.PhysicalDevice = m_physicalDevice;
    initInfo.Device = m_device;
    initInfo.QueueFamily = m_queueFamily;
    initInfo.Queue = m_queue;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.MinImageCount = m_minImageCount;
    initInfo.ImageCount = static_cast<uint32_t>(m_swapchainImages.size());
    initInfo.PipelineInfoMain.RenderPass = m_renderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&initInfo);
    m_imguiInitialized = true;
  }

  void cleanupSwapchain() {
    for (VkFramebuffer framebuffer : m_framebuffers) {
      vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    m_framebuffers.clear();

    if (m_renderPass != VK_NULL_HANDLE) {
      vkDestroyRenderPass(m_device, m_renderPass, nullptr);
      m_renderPass = VK_NULL_HANDLE;
    }

    for (VkImageView imageView : m_swapchainImageViews) {
      vkDestroyImageView(m_device, imageView, nullptr);
    }
    m_swapchainImageViews.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
      m_swapchain = VK_NULL_HANDLE;
    }
  }

  void recreateSwapchain() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    while (width == 0 || height == 0) {
      glfwWaitEvents();
      glfwGetFramebufferSize(m_window, &width, &height);
    }

    vkDeviceWaitIdle(m_device);
    cleanupSwapchain();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createFramebuffers();
    m_framebufferResized = false;
  }

  void renderFrame(EditorModel &model) {
    checkVk(vkWaitForFences(m_device, 1, &m_inFlight, VK_TRUE, UINT64_MAX));
    checkVk(vkResetFences(m_device, 1, &m_inFlight));

    uint32_t imageIndex = 0;
    const VkResult acquireResult =
        vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                              m_imageAvailable, VK_NULL_HANDLE, &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
      recreateSwapchain();
      return;
    }
    checkVk(acquireResult);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    drawEditorUi(model);
    ImGui::Render();

    checkVk(vkResetCommandBuffer(m_commandBuffer, 0));
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    checkVk(vkBeginCommandBuffer(m_commandBuffer, &beginInfo));

    const std::array<VkClearValue, 1> clearValues{
        VkClearValue{{0.06f, 0.07f, 0.08f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchainExtent;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    vkCmdBeginRenderPass(m_commandBuffer, &renderPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffer);
    vkCmdEndRenderPass(m_commandBuffer);
    checkVk(vkEndCommandBuffer(m_commandBuffer));

    const std::array<VkSemaphore, 1> waitSemaphores{m_imageAvailable};
    const std::array<VkPipelineStageFlags, 1> waitStages{
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    const std::array<VkSemaphore, 1> signalSemaphores{m_renderFinished};
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount =
        static_cast<uint32_t>(waitSemaphores.size());
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    submitInfo.signalSemaphoreCount =
        static_cast<uint32_t>(signalSemaphores.size());
    submitInfo.pSignalSemaphores = signalSemaphores.data();
    checkVk(vkQueueSubmit(m_queue, 1, &submitInfo, m_inFlight));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount =
        static_cast<uint32_t>(signalSemaphores.size());
    presentInfo.pWaitSemaphores = signalSemaphores.data();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;
    const VkResult presentResult = vkQueuePresentKHR(m_queue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
        presentResult == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
      recreateSwapchain();
      return;
    }
    checkVk(presentResult);
  }

  void mainLoop(EditorModel &model) {
    uint32_t frameCount = 0;
    while (!glfwWindowShouldClose(m_window)) {
      glfwPollEvents();
      renderFrame(model);
      if (m_options.frames > 0 && ++frameCount >= m_options.frames) {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
      }
    }
  }

  void cleanup() {
    if (m_device != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(m_device);
    }

    if (m_imguiInitialized) {
      ImGui_ImplVulkan_Shutdown();
      ImGui_ImplGlfw_Shutdown();
      ImGui::DestroyContext();
      m_imguiInitialized = false;
    }

    cleanupSwapchain();

    if (m_inFlight != VK_NULL_HANDLE) {
      vkDestroyFence(m_device, m_inFlight, nullptr);
      m_inFlight = VK_NULL_HANDLE;
    }
    if (m_renderFinished != VK_NULL_HANDLE) {
      vkDestroySemaphore(m_device, m_renderFinished, nullptr);
      m_renderFinished = VK_NULL_HANDLE;
    }
    if (m_imageAvailable != VK_NULL_HANDLE) {
      vkDestroySemaphore(m_device, m_imageAvailable, nullptr);
      m_imageAvailable = VK_NULL_HANDLE;
    }
    if (m_commandPool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(m_device, m_commandPool, nullptr);
      m_commandPool = VK_NULL_HANDLE;
    }
    if (m_descriptorPool != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
      m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_device != VK_NULL_HANDLE) {
      vkDestroyDevice(m_device, nullptr);
      m_device = VK_NULL_HANDLE;
    }
    if (m_surface != VK_NULL_HANDLE) {
      vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
      m_surface = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE) {
      vkDestroyInstance(m_instance, nullptr);
      m_instance = VK_NULL_HANDLE;
    }
    if (m_window) {
      glfwDestroyWindow(m_window);
      m_window = nullptr;
    }
    glfwTerminate();
  }
};

void writeEditorResult(const EditorOptions &options, const EditorModel &model,
                       std::string status, std::string message) {
  if (!options.common.resultFile.has_value()) {
    return;
  }

  vac::host::HostResult result =
      vac::host::resultFromOptions("game_editor", options.common);
  result.status = std::move(status);
  result.message = std::move(message);
  result.diagnostics = model.resultDiagnostics();
  vac::host::writeResultFile(*options.common.resultFile, result);
}
} // namespace

int main(int argc, char **argv) {
  std::optional<EditorOptions> parsedOptions;
  try {
    EditorOptions options = parseOptions(argc, argv);
    parsedOptions = options;
    EditorModel model{options};
    model.refreshAll();

    if (!options.common.headless) {
      ImGuiVulkanShell shell{options};
      shell.run(model);
    }

    writeEditorResult(options, model, model.ok() ? "ok" : "error",
                      model.ok() ? "Editor data loaded"
                                 : "Editor data has diagnostics");
    return model.ok() ? 0 : 1;
  } catch (const std::exception &error) {
    std::cerr << "game_editor failed: " << error.what() << '\n';
    if (parsedOptions.has_value() &&
        parsedOptions->common.resultFile.has_value()) {
      try {
        vac::host::HostResult result;
        result.host = "game_editor";
        result.status = "error";
        result.message = error.what();
        vac::host::writeResultFile(*parsedOptions->common.resultFile, result);
      } catch (const std::exception &resultError) {
        std::cerr << "game_editor could not write result file: "
                  << resultError.what() << '\n';
      }
    }
    return 1;
  }
}
