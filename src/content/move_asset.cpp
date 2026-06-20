#include "content/move_asset.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace vac::content
{
namespace
{
using json = nlohmann::json;

glm::vec3 readVec3(const json &value, glm::vec3 fallback)
{
    if (!value.is_array() || value.size() != 3) {
        return fallback;
    }
    return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

json vec3ToJson(glm::vec3 value) { return json::array({value.x, value.y, value.z}); }

std::vector<std::string> readStringVector(const json &value)
{
    std::vector<std::string> result;
    if (!value.is_array()) {
        return result;
    }

    result.reserve(value.size());
    for (const json &item : value) {
        result.push_back(item.get<std::string>());
    }
    return result;
}

std::vector<std::string> sortedStrings(std::vector<std::string> values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

TickRange readRange(const json &value) { return {value.value("beginTick", 0u), value.value("endTick", 0u)}; }

json rangeToJson(TickRange range) { return {{"beginTick", range.begin}, {"endTick", range.end}}; }

InputTrigger readInput(const json &value)
{
    InputTrigger input;
    if (!value.is_object()) {
        return input;
    }
    input.command = value.value("command", "");
    input.bufferTicks = value.value("bufferTicks", 0u);
    return input;
}

json inputToJson(const InputTrigger &input) { return {{"bufferTicks", input.bufferTicks}, {"command", input.command}}; }

MoveResources readResources(const json &value)
{
    MoveResources resources;
    if (!value.is_object()) {
        return resources;
    }
    resources.staminaCost = value.value("staminaCost", 0.0f);
    return resources;
}

json resourcesToJson(const MoveResources &resources) { return {{"staminaCost", resources.staminaCost}}; }

void readHitboxEffect(const json &value, HitboxTrack &track)
{
    if (!value.is_object()) {
        return;
    }
    const json effect = value.value("effect", json::object());
    if (!effect.is_object()) {
        return;
    }
    track.damage = effect.value("damage", 0u);
    track.hitstopTicks = static_cast<uint16_t>(effect.value("hitstopTicks", 0u));
    track.stunTicks = static_cast<uint16_t>(effect.value("stunTicks", 0u));
    track.reactionMove = effect.value("reactionMove", "");
}

json hitboxEffectToJson(const HitboxTrack &track)
{
    return {
        {"damage", track.damage},
        {"hitstopTicks", track.hitstopTicks},
        {"reactionMove", track.reactionMove},
        {"stunTicks", track.stunTicks},
    };
}

bool finiteVec3(glm::vec3 value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }

bool positiveVec3(glm::vec3 value) { return finiteVec3(value) && value.x > 0.0f && value.y > 0.0f && value.z > 0.0f; }

void addError(ValidationResult &result, const MoveAsset &asset, std::string code, std::string component,
              std::string fieldPath, std::string message)
{
    result.diagnostics.push_back({
        "error",
        std::move(code),
        asset.sourcePath.string(),
        asset.logicalId,
        std::move(component),
        std::move(fieldPath),
        std::move(message),
    });
}

bool validRange(TickRange range, uint32_t durationTicks)
{
    return range.begin < range.end && range.end <= durationTicks;
}

void validateRange(ValidationResult &result, const MoveAsset &asset, TickRange range, std::string component,
                   std::string fieldPath)
{
    if (!validRange(range, asset.durationTicks)) {
        addError(result, asset, "invalid_tick_range", std::move(component), std::move(fieldPath),
                 "Tick windows must use half-open [begin, end) ranges inside move duration");
    }
}

void validateTags(ValidationResult &result, const MoveAsset &asset, const std::unordered_set<std::string> &knownTags,
                  const std::vector<std::string> &tags, std::string component, std::string fieldPath)
{
    for (size_t i = 0; i < tags.size(); ++i) {
        if (!knownTags.contains(tags[i])) {
            addError(result, asset, "unknown_tag", component, fieldPath + "/tags/" + std::to_string(i),
                     "Referenced tag is not declared by this move asset");
        }
    }
}

bool validHitboxShape(const std::string &shape) { return shape == "box" || shape == "sphere" || shape == "capsule"; }

uint32_t internIndex(const std::vector<std::string> &values, const std::string &value)
{
    const auto it = std::find(values.begin(), values.end(), value);
    if (it == values.end()) {
        return 0;
    }
    return static_cast<uint32_t>(std::distance(values.begin(), it) + 1);
}

std::vector<uint32_t> internTags(const InternTable &table, std::vector<std::string> tags)
{
    tags = sortedStrings(std::move(tags));
    std::vector<uint32_t> result;
    result.reserve(tags.size());
    for (const std::string &tag : tags) {
        result.push_back(internIndex(table.tagIds, tag));
    }
    return result;
}

template <typename T> void sortById(std::vector<T> &values)
{
    std::sort(values.begin(), values.end(), [](const T &left, const T &right) { return left.id < right.id; });
}
} // namespace

MoveAsset moveAssetFromJson(const nlohmann::json &document, std::filesystem::path sourcePath)
{
    MoveAsset asset;
    asset.sourcePath = std::move(sourcePath);
    asset.schemaVersion = document.value("schemaVersion", 0u);
    asset.assetGuid = document.value("assetGuid", "");
    asset.logicalId = document.value("logicalId", "");
    asset.durationTicks = document.value("durationTicks", 0u);
    asset.tags = readStringVector(document.value("tags", json::array()));
    asset.knownMoves = readStringVector(document.value("knownMoves", json::array()));
    asset.input = readInput(document.value("input", json::object()));
    asset.resources = readResources(document.value("resources", json::object()));

    for (const json &item : document.value("phases", json::array())) {
        asset.phases.push_back(
            {item.value("id", ""), readRange(item), readStringVector(item.value("tags", json::array()))});
    }
    for (const json &item : document.value("movementTracks", json::array())) {
        asset.movementTracks.push_back({item.value("id", ""), readRange(item),
                                        readVec3(item.value("translation", json::array()), {0.0f, 0.0f, 0.0f}),
                                        readStringVector(item.value("tags", json::array()))});
    }
    for (const json &item : document.value("hitboxTracks", json::array())) {
        HitboxTrack track{item.value("id", ""),
                          readRange(item),
                          item.value("shape", ""),
                          item.value("socket", ""),
                          readVec3(item.value("size", json::array()), {1.0f, 1.0f, 1.0f}),
                          readVec3(item.value("offset", json::array()), {0.0f, 0.0f, 0.0f}),
                          readStringVector(item.value("tags", json::array()))};
        readHitboxEffect(item, track);
        asset.hitboxTracks.push_back(std::move(track));
    }
    for (const json &item : document.value("hurtboxOverrides", json::array())) {
        asset.hurtboxOverrides.push_back({item.value("id", ""), readRange(item), item.value("profile", ""),
                                          readStringVector(item.value("tags", json::array()))});
    }
    for (const json &item : document.value("cancels", json::array())) {
        asset.cancels.push_back({item.value("id", ""), readRange(item), item.value("condition", ""),
                                 readStringVector(item.value("destinations", json::array())),
                                 readStringVector(item.value("tags", json::array()))});
    }
    for (const json &item : document.value("events", json::array())) {
        asset.events.push_back({item.value("id", ""), item.value("tick", 0u), item.value("type", ""),
                                item.value("payload", ""), readStringVector(item.value("tags", json::array()))});
    }

    return asset;
}

MoveAsset loadMoveAsset(const std::filesystem::path &path)
{
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error("Could not open move asset '" + path.string() + "'");
    }

    json document;
    input >> document;
    return moveAssetFromJson(document, path);
}

nlohmann::json toCanonicalJson(const MoveAsset &asset)
{
    std::vector<MovePhase> phases = asset.phases;
    std::vector<MovementTrack> movementTracks = asset.movementTracks;
    std::vector<HitboxTrack> hitboxTracks = asset.hitboxTracks;
    std::vector<HurtboxOverride> hurtboxOverrides = asset.hurtboxOverrides;
    std::vector<CancelWindow> cancels = asset.cancels;
    std::vector<MoveEvent> events = asset.events;
    sortById(phases);
    sortById(movementTracks);
    sortById(hitboxTracks);
    sortById(hurtboxOverrides);
    sortById(cancels);
    sortById(events);

    json phaseArray = json::array();
    for (const MovePhase &phase : phases) {
        phaseArray.push_back({{"id", phase.id},
                              {"beginTick", phase.range.begin},
                              {"endTick", phase.range.end},
                              {"tags", sortedStrings(phase.tags)}});
    }

    json movementArray = json::array();
    for (const MovementTrack &track : movementTracks) {
        movementArray.push_back({{"id", track.id},
                                 {"beginTick", track.range.begin},
                                 {"endTick", track.range.end},
                                 {"tags", sortedStrings(track.tags)},
                                 {"translation", vec3ToJson(track.translation)}});
    }

    json hitboxArray = json::array();
    for (const HitboxTrack &track : hitboxTracks) {
        hitboxArray.push_back({{"id", track.id},
                               {"beginTick", track.range.begin},
                               {"endTick", track.range.end},
                               {"effect", hitboxEffectToJson(track)},
                               {"offset", vec3ToJson(track.offset)},
                               {"shape", track.shape},
                               {"size", vec3ToJson(track.size)},
                               {"socket", track.socket},
                               {"tags", sortedStrings(track.tags)}});
    }

    json hurtboxArray = json::array();
    for (const HurtboxOverride &track : hurtboxOverrides) {
        hurtboxArray.push_back({{"id", track.id},
                                {"beginTick", track.range.begin},
                                {"endTick", track.range.end},
                                {"profile", track.profile},
                                {"tags", sortedStrings(track.tags)}});
    }

    json cancelArray = json::array();
    for (const CancelWindow &cancel : cancels) {
        cancelArray.push_back({{"id", cancel.id},
                               {"beginTick", cancel.range.begin},
                               {"endTick", cancel.range.end},
                               {"condition", cancel.condition},
                               {"destinations", sortedStrings(cancel.destinations)},
                               {"tags", sortedStrings(cancel.tags)}});
    }

    json eventArray = json::array();
    for (const MoveEvent &event : events) {
        eventArray.push_back({{"id", event.id},
                              {"payload", event.payload},
                              {"tags", sortedStrings(event.tags)},
                              {"tick", event.tick},
                              {"type", event.type}});
    }

    return {
        {"assetGuid", asset.assetGuid},           {"cancels", std::move(cancelArray)},
        {"durationTicks", asset.durationTicks},   {"events", std::move(eventArray)},
        {"hitboxTracks", std::move(hitboxArray)}, {"hurtboxOverrides", std::move(hurtboxArray)},
        {"input", inputToJson(asset.input)},      {"knownMoves", sortedStrings(asset.knownMoves)},
        {"logicalId", asset.logicalId},           {"movementTracks", std::move(movementArray)},
        {"phases", std::move(phaseArray)},        {"resources", resourcesToJson(asset.resources)},
        {"schemaVersion", asset.schemaVersion},   {"tags", sortedStrings(asset.tags)},
    };
}

std::string toCanonicalJsonString(const MoveAsset &asset) { return toCanonicalJson(asset).dump(2) + "\n"; }

ValidationResult validateMoveAsset(const MoveAsset &asset)
{
    ValidationResult result;
    if (asset.schemaVersion != kMoveAssetSchemaVersion) {
        addError(result, asset, "unsupported_schema_version", "MoveAsset", "schemaVersion",
                 "Unsupported move schema version");
    }
    if (asset.assetGuid.empty()) {
        addError(result, asset, "missing_asset_guid", "MoveAsset", "assetGuid", "Move assetGuid is required");
    }
    if (asset.logicalId.empty()) {
        addError(result, asset, "missing_logical_id", "MoveAsset", "logicalId", "Move logicalId is required");
    }
    if (asset.durationTicks == 0) {
        addError(result, asset, "invalid_duration", "MoveAsset", "durationTicks", "Move duration must be positive");
    }
    if (asset.input.command.empty()) {
        addError(result, asset, "missing_input_command", "InputTrigger", "input/command", "Input command is required");
    }
    if (!std::isfinite(asset.resources.staminaCost) || asset.resources.staminaCost < 0.0f) {
        addError(result, asset, "invalid_resources", "Resources", "resources/staminaCost",
                 "Stamina cost must be non-negative");
    }

    const std::unordered_set<std::string> knownTags{asset.tags.begin(), asset.tags.end()};
    std::unordered_set<std::string> knownMoves{asset.knownMoves.begin(), asset.knownMoves.end()};
    knownMoves.insert(asset.logicalId);

    std::unordered_set<std::string> trackIds;
    auto registerTrackId = [&](std::string_view id, std::string component, std::string fieldPath) {
        if (id.empty()) {
            addError(result, asset, "missing_track_id", component, fieldPath, "Track id is required");
            return;
        }
        if (!trackIds.insert(std::string{id}).second) {
            addError(result, asset, "duplicate_track_id", std::move(component), std::move(fieldPath),
                     "Track id is duplicated");
        }
    };

    for (const MovePhase &phase : asset.phases) {
        validateRange(result, asset, phase.range, "Phase", "phases/" + phase.id);
        validateTags(result, asset, knownTags, phase.tags, "Phase", "phases/" + phase.id);
    }
    for (const MovementTrack &track : asset.movementTracks) {
        registerTrackId(track.id, "MovementTrack", "movementTracks/" + track.id + "/id");
        validateRange(result, asset, track.range, "MovementTrack", "movementTracks/" + track.id);
        validateTags(result, asset, knownTags, track.tags, "MovementTrack", "movementTracks/" + track.id);
        if (!finiteVec3(track.translation)) {
            addError(result, asset, "invalid_movement_track", "MovementTrack",
                     "movementTracks/" + track.id + "/translation", "Movement track translation must be finite");
        }
    }
    for (const HitboxTrack &track : asset.hitboxTracks) {
        registerTrackId(track.id, "HitboxTrack", "hitboxTracks/" + track.id + "/id");
        validateRange(result, asset, track.range, "HitboxTrack", "hitboxTracks/" + track.id);
        validateTags(result, asset, knownTags, track.tags, "HitboxTrack", "hitboxTracks/" + track.id);
        if (!validHitboxShape(track.shape) || track.socket.empty() || !positiveVec3(track.size) ||
            !finiteVec3(track.offset)) {
            addError(result, asset, "invalid_hitbox_track", "HitboxTrack", "hitboxTracks/" + track.id,
                     "Hitbox track shape/socket/size/offset must be valid");
        }
        if (!track.reactionMove.empty() && !knownMoves.contains(track.reactionMove)) {
            addError(result, asset, "unknown_reaction_move", "HitboxTrack",
                     "hitboxTracks/" + track.id + "/effect/reactionMove",
                     "Hitbox effect reaction move is not known");
        }
    }
    for (const HurtboxOverride &track : asset.hurtboxOverrides) {
        registerTrackId(track.id, "HurtboxOverride", "hurtboxOverrides/" + track.id + "/id");
        validateRange(result, asset, track.range, "HurtboxOverride", "hurtboxOverrides/" + track.id);
        validateTags(result, asset, knownTags, track.tags, "HurtboxOverride", "hurtboxOverrides/" + track.id);
        if (track.profile.empty()) {
            addError(result, asset, "invalid_hurtbox_override", "HurtboxOverride",
                     "hurtboxOverrides/" + track.id + "/profile", "Hurtbox override profile is required");
        }
    }

    for (const CancelWindow &cancel : asset.cancels) {
        validateRange(result, asset, cancel.range, "CancelWindow", "cancels/" + cancel.id);
        validateTags(result, asset, knownTags, cancel.tags, "CancelWindow", "cancels/" + cancel.id);
        if (cancel.destinations.empty()) {
            addError(result, asset, "empty_cancel_destinations", "CancelWindow",
                     "cancels/" + cancel.id + "/destinations",
                     "Cancel windows must name at least one destination move");
        }
        for (const std::string &destination : cancel.destinations) {
            if (!knownMoves.contains(destination)) {
                addError(result, asset, "unknown_destination_move", "CancelWindow",
                         "cancels/" + cancel.id + "/destinations", "Cancel destination move is not known");
            }
            if (destination == asset.logicalId && cancel.range.begin == 0) {
                addError(result, asset, "zero_tick_cycle", "CancelWindow", "cancels/" + cancel.id + "/destinations",
                         "Same-move cancels at tick zero would create a zero-tick cycle");
            }
        }
    }

    std::unordered_set<std::string> eventIds;
    for (const MoveEvent &event : asset.events) {
        if (!eventIds.insert(event.id).second) {
            addError(result, asset, "duplicate_event_id", "Event", "events/" + event.id + "/id",
                     "Event id is duplicated");
        }
        if (event.tick >= asset.durationTicks) {
            addError(result, asset, "invalid_event_tick", "Event", "events/" + event.id + "/tick",
                     "Event tick must be inside move duration");
        }
        validateTags(result, asset, knownTags, event.tags, "Event", "events/" + event.id);
    }

    return result;
}

MoveCompileResult compileMoveAsset(const MoveAsset &asset)
{
    MoveCompileResult result;
    result.validation = validateMoveAsset(asset);
    if (!result.validation.ok()) {
        return result;
    }

    result.move.logicalId = asset.logicalId;
    result.move.durationTicks = asset.durationTicks;
    result.move.input = asset.input;
    result.move.resources = asset.resources;
    result.move.internTable.moveIds = sortedStrings(asset.knownMoves);
    if (std::find(result.move.internTable.moveIds.begin(), result.move.internTable.moveIds.end(), asset.logicalId) ==
        result.move.internTable.moveIds.end()) {
        result.move.internTable.moveIds.push_back(asset.logicalId);
        result.move.internTable.moveIds = sortedStrings(result.move.internTable.moveIds);
    }
    result.move.internTable.tagIds = sortedStrings(asset.tags);

    std::vector<std::string> trackIds;
    for (const MovementTrack &track : asset.movementTracks) {
        trackIds.push_back(track.id);
    }
    for (const HitboxTrack &track : asset.hitboxTracks) {
        trackIds.push_back(track.id);
    }
    for (const HurtboxOverride &track : asset.hurtboxOverrides) {
        trackIds.push_back(track.id);
    }
    result.move.internTable.trackIds = sortedStrings(std::move(trackIds));

    std::vector<std::string> eventIds;
    for (const MoveEvent &event : asset.events) {
        eventIds.push_back(event.id);
    }
    result.move.internTable.eventIds = sortedStrings(std::move(eventIds));
    result.move.moveId = internIndex(result.move.internTable.moveIds, asset.logicalId);

    std::vector<MovePhase> phases = asset.phases;
    std::vector<MovementTrack> movementTracks = asset.movementTracks;
    std::vector<HitboxTrack> hitboxTracks = asset.hitboxTracks;
    std::vector<HurtboxOverride> hurtboxOverrides = asset.hurtboxOverrides;
    std::vector<CancelWindow> cancels = asset.cancels;
    std::vector<MoveEvent> events = asset.events;
    sortById(phases);
    sortById(movementTracks);
    sortById(hitboxTracks);
    sortById(hurtboxOverrides);
    sortById(cancels);
    sortById(events);

    for (const MovePhase &phase : phases) {
        result.move.phases.push_back(
            {phase.id, {phase.range.begin, phase.range.end}, internTags(result.move.internTable, phase.tags)});
    }
    for (const MovementTrack &track : movementTracks) {
        result.move.movementTracks.push_back({internIndex(result.move.internTable.trackIds, track.id),
                                              {track.range.begin, track.range.end},
                                              track.translation,
                                              internTags(result.move.internTable, track.tags)});
    }
    for (const HitboxTrack &track : hitboxTracks) {
        result.move.hitboxTracks.push_back({internIndex(result.move.internTable.trackIds, track.id),
                                            {track.range.begin, track.range.end},
                                            track.shape,
                                            track.socket,
                                            track.size,
                                            track.offset,
                                            internTags(result.move.internTable, track.tags),
                                            track.damage,
                                            track.hitstopTicks,
                                            track.stunTicks,
                                            track.reactionMove});
    }
    for (const HurtboxOverride &track : hurtboxOverrides) {
        result.move.hurtboxOverrides.push_back({internIndex(result.move.internTable.trackIds, track.id),
                                                {track.range.begin, track.range.end},
                                                track.profile,
                                                internTags(result.move.internTable, track.tags)});
    }
    for (const CancelWindow &cancel : cancels) {
        std::vector<std::string> destinations = sortedStrings(cancel.destinations);
        std::vector<uint32_t> destinationIds;
        destinationIds.reserve(destinations.size());
        for (const std::string &destination : destinations) {
            destinationIds.push_back(internIndex(result.move.internTable.moveIds, destination));
        }
        result.move.cancels.push_back({cancel.id,
                                       {cancel.range.begin, cancel.range.end},
                                       cancel.condition,
                                       std::move(destinationIds),
                                       internTags(result.move.internTable, cancel.tags)});
    }
    for (const MoveEvent &event : events) {
        result.move.events.push_back({internIndex(result.move.internTable.eventIds, event.id), event.tick, event.type,
                                      event.payload, internTags(result.move.internTable, event.tags)});
    }

    return result;
}
} // namespace vac::content
