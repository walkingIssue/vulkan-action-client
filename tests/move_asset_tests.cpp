#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "content/move_asset.hpp"

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

std::filesystem::path validMovePath() { return projectRoot() / "content/moves/light_attack.move.json"; }

std::filesystem::path invalidFixturePath(std::string_view fileName)
{
    return projectRoot() / "tests/fixtures/moves" / fileName;
}

void expect(bool condition, std::string_view message)
{
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

const vac::content::ContentDiagnostic *findDiagnostic(const vac::content::ValidationResult &result,
                                                      std::string_view code)
{
    for (const vac::content::ContentDiagnostic &diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return &diagnostic;
        }
    }
    return nullptr;
}

void expectDiagnostic(const vac::content::ValidationResult &result, std::string_view code, std::string_view component,
                      std::string_view fieldPath)
{
    const vac::content::ContentDiagnostic *diagnostic = findDiagnostic(result, code);
    expect(diagnostic != nullptr, std::string{"diagnostic exists: "} + std::string{code});
    if (diagnostic == nullptr) {
        return;
    }

    expect(diagnostic->severity == "error", std::string{"diagnostic severity for "} + std::string{code});
    expect(diagnostic->component == component, std::string{"diagnostic component for "} + std::string{code});
    expect(diagnostic->fieldPath == fieldPath, std::string{"diagnostic field for "} + std::string{code});
    expect(!diagnostic->objectId.empty(), std::string{"diagnostic move id for "} + std::string{code});
    expect(!diagnostic->file.empty(), std::string{"diagnostic file for "} + std::string{code});
    expect(!diagnostic->message.empty(), std::string{"diagnostic message for "} + std::string{code});
}

template <typename T> void appendVector(std::ostringstream &output, const std::vector<T> &values)
{
    for (const T &value : values) {
        output << value << ',';
    }
}

std::string compiledSummary(const vac::content::CompiledMove &move)
{
    std::ostringstream output;
    output << "move=" << move.logicalId << ':' << move.moveId << ':' << move.durationTicks << ';';
    output << "moves=";
    appendVector(output, move.internTable.moveIds);
    output << ";tags=";
    appendVector(output, move.internTable.tagIds);
    output << ";tracks=";
    appendVector(output, move.internTable.trackIds);
    output << ";events=";
    appendVector(output, move.internTable.eventIds);

    output << ";phases=";
    for (const vac::content::CompiledPhase &phase : move.phases) {
        output << phase.id << '[' << phase.range.begin << ',' << phase.range.end << "):";
        appendVector(output, phase.tagIds);
    }
    output << ";movement=";
    for (const vac::content::CompiledMovementTrack &track : move.movementTracks) {
        output << track.trackId << '[' << track.range.begin << ',' << track.range.end << "):";
        appendVector(output, track.tagIds);
    }
    output << ";hitboxes=";
    for (const vac::content::CompiledHitboxTrack &track : move.hitboxTracks) {
        output << track.trackId << ':' << track.shape << ':' << track.socket << '[' << track.range.begin << ','
               << track.range.end << "):";
        appendVector(output, track.tagIds);
    }
    output << ";hurtboxes=";
    for (const vac::content::CompiledHurtboxOverride &track : move.hurtboxOverrides) {
        output << track.trackId << ':' << track.profile << '[' << track.range.begin << ',' << track.range.end << "):";
        appendVector(output, track.tagIds);
    }
    output << ";cancels=";
    for (const vac::content::CompiledCancelWindow &cancel : move.cancels) {
        output << cancel.id << ':' << cancel.condition << '[' << cancel.range.begin << ',' << cancel.range.end << "):";
        appendVector(output, cancel.destinationMoveIds);
        output << ':';
        appendVector(output, cancel.tagIds);
    }
    output << ";events=";
    for (const vac::content::CompiledMoveEvent &event : move.events) {
        output << event.eventId << ':' << event.tick << ':' << event.type << ':' << event.payload << ':';
        appendVector(output, event.tagIds);
    }
    return output.str();
}

void validLightAttackCompiles()
{
    const vac::content::MoveAsset asset = vac::content::loadMoveAsset(validMovePath());
    const vac::content::ValidationResult validation = vac::content::validateMoveAsset(asset);
    expect(validation.ok(), "valid light attack validates");

    const vac::content::MoveCompileResult compiled = vac::content::compileMoveAsset(asset);
    expect(compiled.ok(), "valid light attack compiles");
    expect(compiled.move.logicalId == "move.light_attack", "compiled move logical id");
    expect(compiled.move.durationTicks == 34, "compiled move duration");
    expect(compiled.move.moveId == 3, "compiled move id is interned from sorted move IDs");
    expect(compiled.move.internTable.moveIds.size() == 4, "compiled move id table size");
    expect(compiled.move.internTable.trackIds.size() == 3, "compiled track id table size");
    expect(compiled.move.internTable.eventIds.size() == 2, "compiled event id table size");
    expect(compiled.move.phases.size() == 3, "compiled phase count");
    expect(compiled.move.hitboxTracks.size() == 1, "compiled hitbox count");
    expect(compiled.move.hitboxTracks.front().damage == 12, "compiled hitbox damage effect");
    expect(compiled.move.hitboxTracks.front().reactionMove == "move.hit_reaction", "compiled hitbox reaction effect");
    expect(compiled.move.hitboxTracks.front().hitstopTicks == 1, "compiled hitbox hitstop effect");
    expect(compiled.move.hitboxTracks.front().stunTicks == 3, "compiled hitbox stun effect");
    expect(compiled.move.cancels.size() == 2, "compiled cancel count");

    const std::string canonical = vac::content::toCanonicalJsonString(asset);
    const vac::content::MoveAsset canonicalRoundTrip =
        vac::content::moveAssetFromJson(nlohmann::json::parse(canonical), validMovePath());
    expect(vac::content::toCanonicalJsonString(canonicalRoundTrip) == canonical,
           "valid light attack canonical round-trip is stable");
}

void compileIsIndependentOfAuthoringArrayOrder()
{
    vac::content::MoveAsset asset = vac::content::loadMoveAsset(validMovePath());
    const std::string baseline = compiledSummary(vac::content::compileMoveAsset(asset).move);

    std::reverse(asset.tags.begin(), asset.tags.end());
    std::reverse(asset.knownMoves.begin(), asset.knownMoves.end());
    std::reverse(asset.phases.begin(), asset.phases.end());
    std::reverse(asset.movementTracks.begin(), asset.movementTracks.end());
    std::reverse(asset.hitboxTracks.begin(), asset.hitboxTracks.end());
    std::reverse(asset.hurtboxOverrides.begin(), asset.hurtboxOverrides.end());
    std::reverse(asset.cancels.begin(), asset.cancels.end());
    std::reverse(asset.events.begin(), asset.events.end());

    const vac::content::MoveCompileResult reordered = vac::content::compileMoveAsset(asset);
    expect(reordered.ok(), "reordered valid light attack compiles");
    expect(compiledSummary(reordered.move) == baseline, "compiled move table is independent of authoring array order");
}

void invalidFixtureReports(std::string_view fileName, std::string_view code, std::string_view component,
                           std::string_view fieldPath)
{
    const vac::content::MoveAsset asset = vac::content::loadMoveAsset(invalidFixturePath(fileName));
    const vac::content::ValidationResult validation = vac::content::validateMoveAsset(asset);
    expect(!validation.ok(), std::string{"invalid move fixture fails validation: "} + std::string{fileName});
    expectDiagnostic(validation, code, component, fieldPath);

    const vac::content::MoveCompileResult compiled = vac::content::compileMoveAsset(asset);
    expect(!compiled.ok(), std::string{"invalid move fixture does not compile: "} + std::string{fileName});
}

void invalidMovesProduceStructuredDiagnostics()
{
    invalidFixtureReports("invalid_tick_range.move.json", "invalid_tick_range", "Phase", "phases/startup");
    invalidFixtureReports("unknown_tag.move.json", "unknown_tag", "Phase", "phases/startup/tags/1");
    invalidFixtureReports("unknown_destination.move.json", "unknown_destination_move", "CancelWindow",
                          "cancels/missing/destinations");
    invalidFixtureReports("duplicate_track_ids.move.json", "duplicate_track_id", "HitboxTrack",
                          "hitboxTracks/shared_track/id");
    invalidFixtureReports("empty_cancel_destinations.move.json", "empty_cancel_destinations", "CancelWindow",
                          "cancels/empty/destinations");
    invalidFixtureReports("zero_tick_cycle.move.json", "zero_tick_cycle", "CancelWindow",
                          "cancels/self_zero/destinations");
}
} // namespace

int main()
{
    try {
        validLightAttackCompiles();
        compileIsIndependentOfAuthoringArrayOrder();
        invalidMovesProduceStructuredDiagnostics();
    } catch (const std::exception &error) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }

    return g_failures == 0 ? 0 : 1;
}
