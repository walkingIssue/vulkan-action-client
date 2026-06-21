#include "visual_lab/visual_lab.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string_view>
#include <stdexcept>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

#include "animation/proxy_animation.hpp"
#include "combat/combat_collision.hpp"
#include "combat/combat_scenario.hpp"
#include "content/move_asset.hpp"

namespace vac::visual_lab
{
namespace
{
glm::vec3 colorForTeam(std::string_view team)
{
    if (team == "player") {
        return {0.24f, 0.70f, 1.0f};
    }
    if (team == "enemy") {
        return {1.0f, 0.34f, 0.24f};
    }
    return {0.86f, 0.76f, 0.34f};
}

glm::vec3 colorForPhase(std::string_view phase)
{
    if (phase == "startup") {
        return {0.30f, 0.62f, 1.0f};
    }
    if (phase == "active") {
        return {1.0f, 0.30f, 0.22f};
    }
    if (phase == "recovery") {
        return {0.95f, 0.76f, 0.28f};
    }
    return {0.78f, 0.78f, 0.78f};
}

glm::vec3 toVec3(const glm::vec4 &value)
{
    return {value.x, value.y, value.z};
}

Bounds boundsFromContent(const content::Bounds3 &bounds)
{
    return {bounds.min, bounds.max, true};
}

Bounds boundsFromWorld(const content::WorldBounds &bounds)
{
    return {bounds.min, bounds.max, true};
}

glm::vec3 boundsSize(const content::Bounds3 &bounds)
{
    return glm::max(bounds.max - bounds.min, glm::vec3{0.05f});
}

Transform transformFromAuthoring(const content::AuthoringTransform &transform)
{
    Transform result;
    result.translation = transform.translation;
    result.rotationDegrees = transform.rotationDegrees;
    result.scale = transform.scale;
    return result;
}

glm::mat4 transformMatrix(const Transform &transform)
{
    glm::mat4 matrix{1.0f};
    matrix = glm::translate(matrix, transform.translation);
    matrix = glm::rotate(matrix, glm::radians(transform.rotationDegrees.y), glm::vec3{0.0f, 1.0f, 0.0f});
    matrix = glm::rotate(matrix, glm::radians(transform.rotationDegrees.x), glm::vec3{1.0f, 0.0f, 0.0f});
    matrix = glm::rotate(matrix, glm::radians(transform.rotationDegrees.z), glm::vec3{0.0f, 0.0f, 1.0f});
    matrix = glm::scale(matrix, transform.scale);
    return matrix;
}

void appendLine(std::vector<SceneVertex> &vertices, glm::vec3 a, glm::vec3 b, glm::vec3 color)
{
    vertices.push_back({a, {0.0f, 1.0f, 0.0f}, color});
    vertices.push_back({b, {0.0f, 1.0f, 0.0f}, color});
}

void appendBounds(std::vector<SceneVertex> &vertices, Bounds bounds, glm::vec3 color)
{
    if (!bounds.valid) {
        return;
    }

    const std::array<glm::vec3, 8> p = {
        glm::vec3{bounds.min.x, bounds.min.y, bounds.min.z},
        glm::vec3{bounds.max.x, bounds.min.y, bounds.min.z},
        glm::vec3{bounds.max.x, bounds.min.y, bounds.max.z},
        glm::vec3{bounds.min.x, bounds.min.y, bounds.max.z},
        glm::vec3{bounds.min.x, bounds.max.y, bounds.min.z},
        glm::vec3{bounds.max.x, bounds.max.y, bounds.min.z},
        glm::vec3{bounds.max.x, bounds.max.y, bounds.max.z},
        glm::vec3{bounds.min.x, bounds.max.y, bounds.max.z},
    };

    appendLine(vertices, p[0], p[1], color);
    appendLine(vertices, p[1], p[2], color);
    appendLine(vertices, p[2], p[3], color);
    appendLine(vertices, p[3], p[0], color);
    appendLine(vertices, p[4], p[5], color);
    appendLine(vertices, p[5], p[6], color);
    appendLine(vertices, p[6], p[7], color);
    appendLine(vertices, p[7], p[4], color);
    appendLine(vertices, p[0], p[4], color);
    appendLine(vertices, p[1], p[5], color);
    appendLine(vertices, p[2], p[6], color);
    appendLine(vertices, p[3], p[7], color);
}

Bounds centeredBounds(glm::vec3 center, glm::vec3 size)
{
    const glm::vec3 half = size * 0.5f;
    return {center - half, center + half, true};
}

void appendGroundSquare(std::vector<SceneVertex> &vertices, glm::vec3 center, float halfSize, glm::vec3 color)
{
    const glm::vec3 a{center.x - halfSize, center.y, center.z - halfSize};
    const glm::vec3 b{center.x + halfSize, center.y, center.z - halfSize};
    const glm::vec3 c{center.x + halfSize, center.y, center.z + halfSize};
    const glm::vec3 d{center.x - halfSize, center.y, center.z + halfSize};
    appendLine(vertices, a, b, color);
    appendLine(vertices, b, c, color);
    appendLine(vertices, c, d, color);
    appendLine(vertices, d, a, color);
}

void appendSpawnMarker(std::vector<SceneVertex> &vertices, const content::RuntimeSpawnPoint &spawn)
{
    const glm::vec3 color = colorForTeam(spawn.team);
    const glm::vec3 base{spawn.translation.x, spawn.translation.y + 0.08f, spawn.translation.z};
    appendGroundSquare(vertices, base, 0.55f, color);
    appendLine(vertices, base, base + glm::vec3{0.0f, 1.6f, 0.0f}, color);

    const float yaw = glm::radians(spawn.yawDegrees);
    const glm::vec3 forward{std::sin(yaw) * 1.4f, 0.0f, std::cos(yaw) * 1.4f};
    appendLine(vertices, base + glm::vec3{0.0f, 1.0f, 0.0f}, base + glm::vec3{0.0f, 1.0f, 0.0f} + forward, color);
}

void appendActorRoot(std::vector<SceneVertex> &vertices, const simulation::RuntimeActor &actor)
{
    const glm::vec3 root = actor.state.currentTransform.translation + glm::vec3{0.0f, 1.0f, 0.0f};
    appendBounds(vertices, centeredBounds(root, {0.8f, 2.0f, 0.8f}), colorForTeam(actor.team));
}

void appendMovePhaseWindows(std::vector<SceneVertex> &vertices, const content::CompiledMove &move)
{
    constexpr float xOrigin = -8.0f;
    constexpr float zOrigin = 13.0f;
    constexpr float y = 0.12f;
    constexpr float tickScale = 0.26f;
    constexpr float rowStep = 0.42f;

    for (size_t i = 0; i < move.phases.size(); ++i) {
        const content::CompiledPhase &phase = move.phases[i];
        const glm::vec3 color = colorForPhase(phase.id);
        const float z = zOrigin + static_cast<float>(i) * rowStep;
        const float begin = xOrigin + static_cast<float>(phase.range.begin) * tickScale;
        const float end = xOrigin + static_cast<float>(phase.range.end) * tickScale;
        appendLine(vertices, {begin, y, z}, {end, y, z}, color);
        appendLine(vertices, {begin, y, z - 0.12f}, {begin, y + 0.55f, z - 0.12f}, color);
        appendLine(vertices, {end, y, z + 0.12f}, {end, y + 0.55f, z + 0.12f}, color);
    }
}

glm::vec3 firstSpawnOrOrigin(const content::RuntimeWorld &world)
{
    if (!world.spawnPoints.empty()) {
        return world.spawnPoints.front().translation;
    }
    return {0.0f, 0.0f, 0.0f};
}

void appendRootMotionPath(std::vector<SceneVertex> &vertices,
                          const animation::ProxyAnimationAsset &animation,
                          glm::vec3 origin)
{
    if (animation.rootMotion.size() < 2) {
        return;
    }

    for (size_t i = 1; i < animation.rootMotion.size(); ++i) {
        appendLine(vertices,
                   origin + animation.rootMotion[i - 1].translation + glm::vec3{0.0f, 0.08f, 0.0f},
                   origin + animation.rootMotion[i].translation + glm::vec3{0.0f, 0.08f, 0.0f},
                   {0.98f, 0.72f, 0.24f});
    }
}

void appendProxySockets(std::vector<SceneVertex> &vertices,
                        const animation::ProxyAnimationAsset &animation,
                        glm::vec3 origin,
                        float tick)
{
    const animation::ProxyPose pose = animation::sampleProxyPose(animation, tick);
    for (const animation::SocketPose &socket : pose.sockets) {
        const glm::vec3 center = origin + socket.worldTranslation;
        appendLine(vertices, center + glm::vec3{-0.18f, 0.0f, 0.0f}, center + glm::vec3{0.18f, 0.0f, 0.0f},
                   {0.30f, 1.0f, 0.78f});
        appendLine(vertices, center + glm::vec3{0.0f, -0.18f, 0.0f}, center + glm::vec3{0.0f, 0.18f, 0.0f},
                   {0.30f, 1.0f, 0.78f});
        appendLine(vertices, center + glm::vec3{0.0f, 0.0f, -0.18f}, center + glm::vec3{0.0f, 0.0f, 0.18f},
                   {0.30f, 1.0f, 0.78f});
    }
}

void appendInterpolationSamples(std::vector<SceneVertex> &vertices,
                                const animation::ProxyAnimationAsset &animation,
                                glm::vec3 origin)
{
    const std::array<float, 3> ticks = {0.0f, 5.5f, 10.5f};
    for (float tick : ticks) {
        const animation::ProxyPose pose = animation::sampleProxyPose(animation, tick);
        appendGroundSquare(vertices, origin + pose.rootTranslation + glm::vec3{0.0f, 0.16f, 0.0f}, 0.22f,
                           {0.80f, 0.42f, 1.0f});
    }
}

std::vector<combat::SocketTransform> socketTransformsFromPose(const animation::ProxyPose &pose, glm::vec3 origin)
{
    std::vector<combat::SocketTransform> sockets;
    sockets.reserve(pose.sockets.size());
    for (const animation::SocketPose &socket : pose.sockets) {
        sockets.push_back({socket.name, origin + socket.worldTranslation});
    }
    return sockets;
}

combat::CombatVolume volumeFromHitbox(const content::CompiledHitboxTrack &track)
{
    combat::CombatVolume volume;
    volume.binding.socketName = track.socket;
    volume.binding.localOffset = track.offset;
    volume.halfExtents = glm::max(track.size * 0.5f, glm::vec3{0.05f});
    volume.radius = std::max(std::max(track.size.x, track.size.z) * 0.5f, 0.05f);
    volume.halfHeight = std::max(track.size.y * 0.5f, 0.05f);
    if (track.shape == "box") {
        volume.kind = combat::CombatVolumeKind::box;
    } else if (track.shape == "capsule") {
        volume.kind = combat::CombatVolumeKind::capsule;
    } else {
        volume.kind = combat::CombatVolumeKind::sphere;
    }
    return volume;
}

void appendCombatVolumes(std::vector<SceneVertex> &vertices,
                         const content::CompiledMove &move,
                         const animation::ProxyAnimationAsset &animation,
                         glm::vec3 origin)
{
    const animation::ProxyPose pose = animation::sampleProxyPoseAtTick(animation, 10);
    const std::vector<combat::SocketTransform> sockets = socketTransformsFromPose(pose, origin);
    for (const content::CompiledHitboxTrack &track : move.hitboxTracks) {
        const combat::ResolvedVolume resolved = combat::resolveVolume(volumeFromHitbox(track), origin, sockets);
        appendBounds(vertices, centeredBounds(resolved.center, resolved.halfExtents * 2.0f), {1.0f, 0.20f, 0.16f});
    }

    for (const content::CompiledHurtboxOverride &hurtbox : move.hurtboxOverrides) {
        (void)hurtbox;
        appendBounds(vertices, centeredBounds(origin + glm::vec3{0.0f, 1.05f, -1.15f}, {0.9f, 1.8f, 0.9f}),
                     {0.32f, 0.86f, 1.0f});
    }
}

SceneRuntime buildSceneRuntime(const content::RuntimeWorld &world, const simulation::RuntimeWorld &simulationWorld)
{
    SceneRuntime scene;
    scene.name = "sprint01_visual_lab";
    scene.worldBounds = boundsFromWorld(world.worldBounds);

    for (const content::RuntimeEntity &entity : world.entities) {
        ProceduralInstance primitive;
        primitive.id = entity.authoredId.value;
        primitive.name = entity.authoredId.value;
        primitive.type = content::toString(entity.primitiveKind);
        primitive.size3 = boundsSize(entity.localBounds);
        primitive.size = {primitive.size3.x, primitive.size3.z};
        primitive.baseColor = entity.baseColor;
        primitive.transform = transformFromAuthoring(entity.transform);
        primitive.worldBounds = transformBounds(boundsFromContent(entity.localBounds), primitive.transform);
        scene.procedural.push_back(std::move(primitive));
    }

    for (const simulation::RuntimeActor &actor : simulationWorld.actors) {
        ProceduralInstance root;
        root.id = "actor_root_" + actor.spawnId;
        root.name = root.id;
        root.type = "actor_root";
        root.size3 = {0.8f, 2.0f, 0.8f};
        root.size = {root.size3.x, root.size3.z};
        root.baseColor = glm::vec4{colorForTeam(actor.team), 0.72f};
        root.transform = actor.state.currentTransform;
        root.transform.translation.y += 1.0f;
        root.worldBounds = transformBounds({root.size3 * -0.5f, root.size3 * 0.5f, true}, root.transform);
        scene.procedural.push_back(std::move(root));
    }

    refreshSceneBounds(scene);
    return scene;
}

void addDiagnostic(std::vector<std::string> &diagnostics, const content::ContentDiagnostic &diagnostic)
{
    diagnostics.push_back(diagnostic.code + ": " + diagnostic.message);
}

void addScenarioDiagnostic(std::vector<std::string> &diagnostics, const combat::ScenarioDiagnostic &diagnostic)
{
    diagnostics.push_back("scenario:" + diagnostic.code + ":" + diagnostic.fieldPath + ": " + diagnostic.message);
}

std::string boolString(bool value)
{
    return value ? "true" : "false";
}

uint32_t clampedPlaybackTick(const VisualLabPlaybackState &state, uint32_t tick)
{
    return std::min(tick, state.durationTicks);
}

std::string joinStrings(const std::vector<std::string> &values, std::string_view separator)
{
    std::string result;
    for (const std::string &value : values) {
        if (!result.empty()) {
            result += separator;
        }
        result += value;
    }
    return result;
}

void appendScenarioResultDiagnostics(std::vector<std::string> &diagnostics,
                                     const combat::ScenarioRunResult &scenarioResult)
{
    const std::string scenarioId = scenarioResult.trace.scenarioId.empty()
        ? scenarioResult.scenario.logicalId
        : scenarioResult.trace.scenarioId;
    diagnostics.push_back("visualLabSource=scenario");
    diagnostics.push_back("scenarioId=" + scenarioId);
    diagnostics.push_back("scenarioStatus=" + scenarioResult.status);
    diagnostics.push_back("scenarioGoldenCompared=" + boolString(scenarioResult.goldenCompared));
    diagnostics.push_back("scenarioGoldenMatched=" + boolString(scenarioResult.goldenMatched));
    diagnostics.push_back("scenarioTraceEventCount=" + std::to_string(scenarioResult.trace.events.size()));
    diagnostics.push_back("scenarioFinalStateHash=" + scenarioResult.trace.finalStateHash);
    diagnostics.push_back("scenarioTicksRun=" + std::to_string(scenarioResult.trace.ticksRun));
    diagnostics.push_back("scenarioGoldenPath=" + scenarioResult.goldenPath.generic_string());
    if (!scenarioResult.message.empty()) {
        diagnostics.push_back("scenarioMessage=" + scenarioResult.message);
    }
    for (const combat::ScenarioDiagnostic &diagnostic : scenarioResult.diagnostics) {
        addScenarioDiagnostic(diagnostics, diagnostic);
    }
}

void appendPlaybackAndEvidenceDiagnostics(std::vector<std::string> &diagnostics,
                                          const VisualLabPlaybackState &playback,
                                          const VisualLabScenarioEvidenceSummary &evidence)
{
    std::vector<std::string> playbackValues = playbackDiagnostics(playback);
    diagnostics.insert(diagnostics.end(), playbackValues.begin(), playbackValues.end());

    VisualLabPlaybackState previewStep = stepPlayback(playback);
    diagnostics.push_back("playbackPreviewStepTick=" + std::to_string(previewStep.currentTick));
    diagnostics.push_back("playbackPreviewResetTick=" + std::to_string(resetPlayback(previewStep).currentTick));
    diagnostics.push_back("playbackPreviewSeekEndTick=" +
                          std::to_string(seekPlayback(playback, playback.durationTicks + 1000u).currentTick));

    std::vector<std::string> evidenceValues = scenarioEvidenceDiagnostics(evidence);
    diagnostics.insert(diagnostics.end(), evidenceValues.begin(), evidenceValues.end());
}

void populateVisualLabScene(VisualLabScene &result,
                            const content::RuntimeWorld &world,
                            const content::CompiledMove &move,
                            const animation::ProxyAnimationAsset &animation,
                            const simulation::RuntimeWorld &simulationWorld)
{
    result.scene = buildSceneRuntime(world, simulationWorld);

    result.summary.primitiveCount = static_cast<uint32_t>(world.entities.size());
    result.summary.colliderCount = static_cast<uint32_t>(world.colliders.size());
    result.summary.spawnPointCount = static_cast<uint32_t>(world.spawnPoints.size());
    result.summary.actorRootCount = static_cast<uint32_t>(simulationWorld.actors.size());
    result.summary.movePhaseWindowCount = static_cast<uint32_t>(move.phases.size());
    result.summary.hitboxVolumeCount = static_cast<uint32_t>(move.hitboxTracks.size());
    result.summary.hurtboxVolumeCount = static_cast<uint32_t>(move.hurtboxOverrides.size());
    result.summary.proxySocketMarkerCount =
        static_cast<uint32_t>(animation::sampleProxyPoseAtTick(animation, 10).sockets.size());
    result.summary.rootMotionPathSegmentCount =
        animation.rootMotion.empty() ? 0u : static_cast<uint32_t>(animation.rootMotion.size() - 1);
    result.summary.interpolationSampleCount = 3;

    appendBounds(result.debugDraw.lineVertices, boundsFromWorld(world.worldBounds), {0.40f, 0.95f, 0.72f});
    for (const content::RuntimeSpawnPoint &spawn : world.spawnPoints) {
        appendSpawnMarker(result.debugDraw.lineVertices, spawn);
    }
    for (const simulation::RuntimeActor &actor : simulationWorld.actors) {
        appendActorRoot(result.debugDraw.lineVertices, actor);
    }

    const glm::vec3 origin = firstSpawnOrOrigin(world);
    appendMovePhaseWindows(result.debugDraw.lineVertices, move);
    appendCombatVolumes(result.debugDraw.lineVertices, move, animation, origin);
    appendRootMotionPath(result.debugDraw.lineVertices, animation, origin);
    appendProxySockets(result.debugDraw.lineVertices, animation, origin, 10.0f);
    appendInterpolationSamples(result.debugDraw.lineVertices, animation, origin);

    result.summary.debugLineVertexCount = static_cast<uint32_t>(result.debugDraw.lineVertices.size());
}
} // namespace

VisualLabAssetPaths defaultVisualLabAssetPaths()
{
    const std::filesystem::path root = defaultProjectRoot();
    return {
        root / "content/maps/basic_primitive_arena.map.json",
        root / "content/moves/light_attack.move.json",
        root / "content/animations/sword_light_proxy.anim.json",
    };
}

VisualLabScene buildVisualLabScene(const VisualLabAssetPaths &paths)
{
    VisualLabScene result;

    const content::AuthoringScene authoringScene = content::loadAuthoringScene(paths.mapPath);
    const content::CompileResult mapCompile = content::compileRuntimeWorld(authoringScene);
    for (const content::ContentDiagnostic &diagnostic : mapCompile.validation.diagnostics) {
        addDiagnostic(result.diagnostics, diagnostic);
    }
    if (!mapCompile.ok()) {
        return result;
    }

    const content::MoveAsset moveAsset = content::loadMoveAsset(paths.movePath);
    const content::MoveCompileResult moveCompile = content::compileMoveAsset(moveAsset);
    for (const content::ContentDiagnostic &diagnostic : moveCompile.validation.diagnostics) {
        addDiagnostic(result.diagnostics, diagnostic);
    }
    if (!moveCompile.ok()) {
        return result;
    }

    const animation::ProxyAnimationAsset animation = animation::loadProxyAnimation(paths.proxyAnimationPath);
    const content::ValidationResult animationValidation = animation::validateProxyAnimation(animation);
    for (const content::ContentDiagnostic &diagnostic : animationValidation.diagnostics) {
        addDiagnostic(result.diagnostics, diagnostic);
    }
    if (!animationValidation.ok()) {
        return result;
    }

    simulation::RuntimeWorld simulationWorld = simulation::importRuntimeWorld(mapCompile.world);
    populateVisualLabScene(result, mapCompile.world, moveCompile.move, animation, simulationWorld);
    return result;
}

VisualLabScene buildVisualLabSceneFromScenario(const std::filesystem::path &scenarioPath)
{
    VisualLabScene result;

    combat::CombatScenario scenario = combat::loadCombatScenario(scenarioPath);
    const combat::ScenarioRunResult scenarioResult = combat::runCombatScenario(scenario);
    appendScenarioResultDiagnostics(result.resultDiagnostics, scenarioResult);
    result.playback = makePlaybackState(scenarioResult.trace.ticksRun);
    result.scenarioEvidence = summarizeScenarioEvidence(scenarioResult.trace);
    appendPlaybackAndEvidenceDiagnostics(result.resultDiagnostics, result.playback, result.scenarioEvidence);
    if (scenarioResult.status != "ok") {
        if (scenarioResult.diagnostics.empty()) {
            result.diagnostics.push_back("scenario: " + scenarioResult.message);
        } else {
            for (const combat::ScenarioDiagnostic &diagnostic : scenarioResult.diagnostics) {
                addScenarioDiagnostic(result.diagnostics, diagnostic);
            }
        }
        return result;
    }

    scenario = scenarioResult.scenario;
    const combat::CombatScenarioResolvedPaths paths = combat::resolveCombatScenarioPaths(scenario);

    const content::AuthoringScene authoringScene = content::loadAuthoringScene(paths.mapPath);
    const content::CompileResult mapCompile = content::compileRuntimeWorld(authoringScene);
    for (const content::ContentDiagnostic &diagnostic : mapCompile.validation.diagnostics) {
        addDiagnostic(result.diagnostics, diagnostic);
    }
    if (!mapCompile.ok()) {
        return result;
    }

    std::vector<content::CompiledMove> moves;
    moves.reserve(paths.movePaths.size());
    for (const std::filesystem::path &movePath : paths.movePaths) {
        const content::MoveAsset moveAsset = content::loadMoveAsset(movePath);
        const content::MoveCompileResult moveCompile = content::compileMoveAsset(moveAsset);
        for (const content::ContentDiagnostic &diagnostic : moveCompile.validation.diagnostics) {
            addDiagnostic(result.diagnostics, diagnostic);
        }
        if (!moveCompile.ok()) {
            return result;
        }
        moves.push_back(moveCompile.move);
    }
    if (moves.empty()) {
        result.diagnostics.push_back("scenario_visual_lab_missing_move: Scenario has no move assets");
        return result;
    }

    std::unordered_map<std::string, std::filesystem::path> animationPathsByMove;
    for (const combat::ScenarioResolvedAnimationPath &binding : paths.animations) {
        animationPathsByMove[binding.moveLogicalId] = binding.path;
    }

    size_t selectedMoveIndex = 0;
    std::filesystem::path selectedAnimationPath;
    for (size_t i = 0; i < moves.size(); ++i) {
        const auto animationIt = animationPathsByMove.find(moves[i].logicalId);
        if (animationIt != animationPathsByMove.end() && !moves[i].hitboxTracks.empty()) {
            selectedMoveIndex = i;
            selectedAnimationPath = animationIt->second;
            break;
        }
    }
    if (selectedAnimationPath.empty()) {
        for (size_t i = 0; i < moves.size(); ++i) {
            const auto animationIt = animationPathsByMove.find(moves[i].logicalId);
            if (animationIt != animationPathsByMove.end()) {
                selectedMoveIndex = i;
                selectedAnimationPath = animationIt->second;
                break;
            }
        }
    }
    if (selectedAnimationPath.empty()) {
        result.diagnostics.push_back("scenario_visual_lab_missing_animation: Scenario has no proxy animation binding");
        return result;
    }

    const animation::ProxyAnimationAsset animation = animation::loadProxyAnimation(selectedAnimationPath);
    const content::ValidationResult animationValidation = animation::validateProxyAnimation(animation);
    for (const content::ContentDiagnostic &diagnostic : animationValidation.diagnostics) {
        addDiagnostic(result.diagnostics, diagnostic);
    }
    if (!animationValidation.ok()) {
        return result;
    }

    const simulation::RuntimeWorld simulationWorld = combat::makeScenarioRuntimeWorld(scenario, mapCompile.world);
    populateVisualLabScene(result, mapCompile.world, moves[selectedMoveIndex], animation, simulationWorld);
    return result;
}

VisualLabPlaybackState makePlaybackState(uint32_t durationTicks)
{
    VisualLabPlaybackState state;
    state.durationTicks = durationTicks;
    state.currentTick = 0;
    state.paused = true;
    state.running = false;
    return state;
}

VisualLabPlaybackState setPlaybackPaused(VisualLabPlaybackState state, bool paused)
{
    state.paused = paused;
    state.running = !paused;
    state.currentTick = clampedPlaybackTick(state, state.currentTick);
    return state;
}

VisualLabPlaybackState resetPlayback(VisualLabPlaybackState state)
{
    state.currentTick = 0;
    state.paused = true;
    state.running = false;
    return state;
}

VisualLabPlaybackState stepPlayback(VisualLabPlaybackState state, uint32_t ticks)
{
    const uint32_t remaining = state.durationTicks - clampedPlaybackTick(state, state.currentTick);
    state.currentTick += std::min(ticks, remaining);
    state.currentTick = clampedPlaybackTick(state, state.currentTick);
    state.paused = true;
    state.running = false;
    return state;
}

VisualLabPlaybackState seekPlayback(VisualLabPlaybackState state, uint32_t tick)
{
    state.currentTick = clampedPlaybackTick(state, tick);
    return state;
}

VisualLabScenarioEvidenceSummary summarizeScenarioEvidence(const combat::ScenarioTrace &trace)
{
    VisualLabScenarioEvidenceSummary summary;
    summary.eventCount = static_cast<uint32_t>(trace.events.size());
    if (!trace.events.empty()) {
        summary.firstEventTick = trace.events.front().tick;
        summary.lastEventTick = trace.events.front().tick;
    }

    uint32_t lowestRemainingHealth = std::numeric_limits<uint32_t>::max();
    for (const combat::ScenarioTraceEvent &event : trace.events) {
        summary.firstEventTick = std::min(summary.firstEventTick, event.tick);
        summary.lastEventTick = std::max(summary.lastEventTick, event.tick);
        if (event.kind == "hit") {
            ++summary.hitEventCount;
        }
        if (event.kind == "hit_blocked") {
            ++summary.blockedHitEventCount;
        }
        if (event.hasEffect) {
            ++summary.effectEventCount;
            summary.totalDamage += event.damage;
            summary.maxDamage = std::max(summary.maxDamage, event.damage);
            lowestRemainingHealth = std::min(lowestRemainingHealth, event.targetRemainingHealth);
            if (!event.reactionMove.empty() &&
                std::find(summary.reactionMoves.begin(), summary.reactionMoves.end(), event.reactionMove) ==
                    summary.reactionMoves.end()) {
                summary.reactionMoves.push_back(event.reactionMove);
            }
        }
    }
    summary.lowestRemainingHealth =
        summary.effectEventCount == 0 ? 0u : lowestRemainingHealth;
    return summary;
}

std::vector<std::string> summaryDiagnostics(const VisualLabSummary &summary)
{
    return {
        "visualLab=true",
        "primitiveCount=" + std::to_string(summary.primitiveCount),
        "colliderCount=" + std::to_string(summary.colliderCount),
        "spawnPointCount=" + std::to_string(summary.spawnPointCount),
        "actorRootCount=" + std::to_string(summary.actorRootCount),
        "movePhaseWindowCount=" + std::to_string(summary.movePhaseWindowCount),
        "hitboxVolumeCount=" + std::to_string(summary.hitboxVolumeCount),
        "hurtboxVolumeCount=" + std::to_string(summary.hurtboxVolumeCount),
        "proxySocketMarkerCount=" + std::to_string(summary.proxySocketMarkerCount),
        "rootMotionPathSegmentCount=" + std::to_string(summary.rootMotionPathSegmentCount),
        "interpolationSampleCount=" + std::to_string(summary.interpolationSampleCount),
        "debugLineVertexCount=" + std::to_string(summary.debugLineVertexCount),
    };
}

std::vector<std::string> playbackDiagnostics(const VisualLabPlaybackState &state)
{
    return {
        "visualLabPlayback=true",
        "playbackCurrentTick=" + std::to_string(state.currentTick),
        "playbackDurationTicks=" + std::to_string(state.durationTicks),
        "playbackPaused=" + boolString(state.paused),
        "playbackRunning=" + boolString(state.running),
        "playbackCanStepForward=" + boolString(state.currentTick < state.durationTicks),
    };
}

std::vector<std::string> scenarioEvidenceDiagnostics(const VisualLabScenarioEvidenceSummary &summary)
{
    return {
        "scenarioEvidenceEventCount=" + std::to_string(summary.eventCount),
        "scenarioEvidenceFirstTick=" + std::to_string(summary.firstEventTick),
        "scenarioEvidenceLastTick=" + std::to_string(summary.lastEventTick),
        "scenarioEvidenceEffectEventCount=" + std::to_string(summary.effectEventCount),
        "scenarioEvidenceHitEventCount=" + std::to_string(summary.hitEventCount),
        "scenarioEvidenceBlockedHitEventCount=" + std::to_string(summary.blockedHitEventCount),
        "scenarioEvidenceTotalDamage=" + std::to_string(summary.totalDamage),
        "scenarioEvidenceMaxDamage=" + std::to_string(summary.maxDamage),
        "scenarioEvidenceLowestRemainingHealth=" + std::to_string(summary.lowestRemainingHealth),
        "scenarioEvidenceHasDamage=" + boolString(summary.totalDamage > 0),
        "scenarioEvidenceHasReaction=" + boolString(!summary.reactionMoves.empty()),
        "scenarioEvidenceReactionMoves=" + joinStrings(summary.reactionMoves, ","),
    };
}
} // namespace vac::visual_lab
