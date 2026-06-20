#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "animation/proxy_animation.hpp"

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

void expectNear(float actual, float expected, float tolerance, std::string_view message)
{
    if (std::fabs(actual - expected) > tolerance) {
        ++g_failures;
        std::cerr << "FAIL: " << message << " expected " << expected << " got " << actual << '\n';
    }
}

void expectVec3(glm::vec3 actual, glm::vec3 expected, float tolerance, std::string_view message)
{
    expectNear(actual.x, expected.x, tolerance, std::string{message} + " x");
    expectNear(actual.y, expected.y, tolerance, std::string{message} + " y");
    expectNear(actual.z, expected.z, tolerance, std::string{message} + " z");
}

vac::animation::ProxyAnimationAsset loadFixture()
{
    return vac::animation::loadProxyAnimation(projectRoot() / "content/animations/sword_light_proxy.anim.json");
}

void validatesAndCanonicalRoundTrips()
{
    const vac::animation::ProxyAnimationAsset asset = loadFixture();
    const vac::content::ValidationResult validation = vac::animation::validateProxyAnimation(asset);
    expect(validation.ok(), "proxy animation fixture validates");

    const std::string canonical = vac::animation::toCanonicalJsonString(asset);
    const vac::animation::ProxyAnimationAsset roundTrip =
        vac::animation::proxyAnimationFromJson(nlohmann::json::parse(canonical), asset.sourcePath);
    expect(vac::animation::toCanonicalJsonString(roundTrip) == canonical, "proxy animation canonical round-trips");
}

void samplesExactKeyframes()
{
    const vac::animation::ProxyPose pose = vac::animation::sampleProxyPoseAtTick(loadFixture(), 10);
    expectVec3(pose.rootTranslation, {0.0f, 0.0f, 2.0f}, 0.001f, "exact root translation");
    expectNear(pose.rootYawDegrees, 45.0f, 0.001f, "exact root yaw");

    const vac::animation::SocketPose *tip = vac::animation::findSocket(pose, "weapon_tip");
    expect(tip != nullptr, "weapon_tip socket exists");
    if (tip != nullptr) {
        expectVec3(tip->localTranslation, {1.0f, 1.3f, 2.0f}, 0.001f, "exact weapon_tip local translation");
        expectNear(tip->localRotationDegrees.y, 35.0f, 0.001f, "exact weapon_tip local yaw");
    }
}

void interpolatesMidpoints()
{
    const vac::animation::ProxyPose pose = vac::animation::sampleProxyPose(loadFixture(), 5.0f);
    expectVec3(pose.rootTranslation, {0.0f, 0.0f, 1.0f}, 0.001f, "midpoint root translation");
    expectNear(pose.rootYawDegrees, 22.5f, 0.001f, "midpoint root yaw");

    const vac::animation::SocketPose *tip = vac::animation::findSocket(pose, "weapon_tip");
    expect(tip != nullptr, "midpoint weapon_tip socket exists");
    if (tip != nullptr) {
        expectVec3(tip->localTranslation, {0.5f, 1.15f, 1.75f}, 0.001f, "midpoint weapon_tip local translation");
        expectNear(tip->localRotationDegrees.y, 17.5f, 0.001f, "midpoint weapon_tip yaw");
    }
}

void accumulatesRootMotion()
{
    const vac::animation::ProxyAnimationAsset asset = loadFixture();
    expectVec3(vac::animation::rootMotionDelta(asset, 0.0f, 20.0f), {0.0f, 0.0f, 4.0f}, 0.001f,
               "full root motion delta");
    expectVec3(vac::animation::rootMotionDelta(asset, 5.0f, 15.0f), {0.0f, 0.0f, 2.0f}, 0.001f,
               "partial root motion delta");
}

void computesSocketWorldTransforms()
{
    const vac::animation::ProxyPose pose = vac::animation::sampleProxyPoseAtTick(loadFixture(), 10);
    const vac::animation::SocketPose *tip = vac::animation::findSocket(pose, "weapon_tip");
    const vac::animation::SocketPose *base = vac::animation::findSocket(pose, "weapon_base");
    const vac::animation::SocketPose *chest = vac::animation::findSocket(pose, "chest");
    expect(tip != nullptr && base != nullptr && chest != nullptr, "all required sockets exist");
    if (tip == nullptr || base == nullptr || chest == nullptr) {
        return;
    }

    expectVec3(tip->worldTranslation, {2.12132f, 1.3f, 2.70711f}, 0.001f, "weapon_tip world translation");
    expectNear(tip->worldRotationDegrees.y, 80.0f, 0.001f, "weapon_tip world yaw");
    expectNear(base->worldRotationDegrees.y, 65.0f, 0.001f, "weapon_base world yaw");
    expectVec3(chest->worldTranslation, {0.0f, 1.5f, 2.0f}, 0.001f, "chest world translation");
}

void integerTickSamplingMatchesPresentationAtSameTick()
{
    const vac::animation::ProxyAnimationAsset asset = loadFixture();
    const vac::animation::ProxyPose headless = vac::animation::sampleProxyPoseAtTick(asset, 10);
    const vac::animation::ProxyPose presentation = vac::animation::sampleProxyPose(asset, 10.0f);
    expectVec3(headless.rootTranslation, presentation.rootTranslation, 0.001f,
               "headless and presentation root sample match");
    const vac::animation::SocketPose *headlessTip = vac::animation::findSocket(headless, "weapon_tip");
    const vac::animation::SocketPose *presentationTip = vac::animation::findSocket(presentation, "weapon_tip");
    expect(headlessTip != nullptr && presentationTip != nullptr, "matching weapon_tip samples exist");
    if (headlessTip != nullptr && presentationTip != nullptr) {
        expectVec3(headlessTip->worldTranslation, presentationTip->worldTranslation, 0.001f,
                   "headless and presentation socket sample match");
    }
}
} // namespace

int main()
{
    try {
        validatesAndCanonicalRoundTrips();
        samplesExactKeyframes();
        interpolatesMidpoints();
        accumulatesRootMotion();
        computesSocketWorldTransforms();
        integerTickSamplingMatchesPresentationAtSameTick();
    } catch (const std::exception &error) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }

    return g_failures == 0 ? 0 : 1;
}
