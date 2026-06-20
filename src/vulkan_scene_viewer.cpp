#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <Windows.h>
#include <Xinput.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "combat/combat_simulation.hpp"
#include "config/control_profile.hpp"
#include "host/host_cli.hpp"
#include "input/glfw_control_bindings.hpp"
#include "network/snapshot_client.hpp"
#include "render/scene_geometry.hpp"
#include "scene/scene_runtime.hpp"
#include "visual_lab/visual_lab.hpp"

namespace
{
constexpr uint32_t kWidth = 1280;
constexpr uint32_t kHeight = 720;
constexpr int kMaxFramesInFlight = 1;
constexpr float kDefaultCameraDistanceWorldUnits = 42.0f;
constexpr float kMaxCameraDistanceWorldUnits = kDefaultCameraDistanceWorldUnits * 3.0f;
constexpr float kMinCameraDistanceWorldUnits = 0.0f;
constexpr float kCameraScrollZoomStepWorldUnits = 6.0f;
constexpr float kCameraFirstPersonBlendDistanceWorldUnits = 10.0f;

const std::vector<const char *> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

#ifdef NDEBUG
constexpr bool kEnableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif

struct ViewerOptions
{
    std::filesystem::path scenePath;
    std::filesystem::path controlProfilePath;
    std::string networkServer = "127.0.0.1:40000";
    uint8_t networkClientId = 0;
    uint32_t frames = 0;
    bool orbitCamera = false;
    bool visualLab = false;
    std::filesystem::path visualLabMovePath;
    std::filesystem::path visualLabProxyAnimationPath;
    vac::host::CommonHostOptions common;
};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool complete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct Buffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};

struct Image
{
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

struct ModelBuffer
{
    Buffer buffer;
    uint32_t vertexCount = 0;
};

struct PushConstants
{
    glm::mat4 modelViewProjection{1.0f};
    glm::vec4 color{1.0f};
};

struct CameraView
{
    glm::vec3 eye{0.0f};
    glm::vec3 target{0.0f};
    float worldRadius = 32.0f;
};

std::vector<char> readFile(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) {
        throw std::runtime_error(fmt::format("Could not open '{}'", path.string()));
    }

    const auto size = file.tellg();
    std::vector<char> buffer(static_cast<size_t>(size));
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}

ViewerOptions parseOptions(int argc, char **argv)
{
    vac::host::CommandLine commandLine{argc, argv};
    ViewerOptions options;
    options.common = vac::host::parseCommonOptions(commandLine);
    vac::host::rejectUnsupportedCommonOptions(
        options.common, {"--scene", "--frames", "--ticks", "--result-file", "--offline", "--hidden-window"});

    if (commandLine.consumeFlag("--visual-lab")) {
        options.visualLab = true;
    }

    const vac::visual_lab::VisualLabAssetPaths labDefaults = vac::visual_lab::defaultVisualLabAssetPaths();
    options.scenePath = options.visualLab
        ? labDefaults.mapPath
        : vac::defaultProjectRoot() / "config/scenes/bootstrap.scene.json";
    options.controlProfilePath = vac::defaultControlProfilePath();
    options.visualLabMovePath = labDefaults.movePath;
    options.visualLabProxyAnimationPath = labDefaults.proxyAnimationPath;

    if (options.common.scene.has_value()) {
        options.scenePath = *options.common.scene;
    }
    if (options.common.frames.has_value()) {
        options.frames = *options.common.frames;
    }

    if (const std::optional<std::string> controlProfile = commandLine.consumeValue("--control-profile")) {
        options.controlProfilePath = *controlProfile;
    }
    if (const std::optional<std::string> movePath = commandLine.consumeValue("--move")) {
        if (!options.visualLab) {
            throw vac::host::ParseError("--move requires --visual-lab");
        }
        options.visualLabMovePath = *movePath;
    }
    if (const std::optional<std::string> animationPath = commandLine.consumeValue("--proxy-animation")) {
        if (!options.visualLab) {
            throw vac::host::ParseError("--proxy-animation requires --visual-lab");
        }
        options.visualLabProxyAnimationPath = *animationPath;
    }
    if (const std::optional<std::string> networkServer = commandLine.consumeValue("--net-server")) {
        options.networkServer = *networkServer;
    }
    if (const std::optional<std::string> clientIdValue = commandLine.consumeValue("--net-client-id")) {
        const int clientId = std::stoi(*clientIdValue);
        if (clientId < 0 || clientId > 255) {
            throw std::runtime_error("--net-client-id must be between 0 and 255");
        }
        options.networkClientId = static_cast<uint8_t>(clientId);
    }
    if (commandLine.consumeFlag("--orbit-camera")) {
        options.orbitCamera = true;
    }
    if (commandLine.consumeFlag("--static-camera")) {
        options.orbitCamera = false;
    }
    commandLine.rejectUnknown();

    return options;
}

std::optional<std::filesystem::file_time_type> fileWriteTime(const std::filesystem::path &path)
{
    std::error_code error;
    const auto writeTime = std::filesystem::last_write_time(path, error);
    if (error) {
        return std::nullopt;
    }
    return writeTime;
}

VkVertexInputBindingDescription vertexBindingDescription()
{
    return {
        0,
        sizeof(vac::SceneVertex),
        VK_VERTEX_INPUT_RATE_VERTEX,
    };
}

std::array<VkVertexInputAttributeDescription, 3> vertexAttributeDescriptions()
{
    return {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vac::SceneVertex, position)},
        VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vac::SceneVertex, normal)},
        VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vac::SceneVertex, color)},
    };
}

class VulkanSceneViewer
{
public:
    explicit VulkanSceneViewer(ViewerOptions options)
        : m_options(std::move(options))
    {
        loadInitialControlProfile();
        if (m_options.visualLab) {
            vac::visual_lab::VisualLabAssetPaths paths;
            paths.mapPath = m_options.scenePath;
            paths.movePath = m_options.visualLabMovePath;
            paths.proxyAnimationPath = m_options.visualLabProxyAnimationPath;
            const vac::visual_lab::VisualLabScene labScene = vac::visual_lab::buildVisualLabScene(paths);
            if (!labScene.diagnostics.empty()) {
                throw std::runtime_error("Visual lab asset validation failed: " + labScene.diagnostics.front());
            }
            m_scene = labScene.scene;
            m_lineData = labScene.debugDraw;
            m_resultDiagnostics = vac::visual_lab::summaryDiagnostics(labScene.summary);
        } else {
            m_scene = vac::loadScene(m_options.scenePath, vac::defaultProjectRoot());
            m_lineData = vac::buildSceneLineData(m_scene);
        }
        findControllableActors();
        configureNetworkActors();
        vac::refreshSceneBounds(m_scene);
        m_renderData = vac::buildSceneRenderData(m_scene);
        initializeCombatActors();
        initializeCameraAnchor();
        initializeNetworkClient();
    }

    ~VulkanSceneViewer()
    {
        cleanup();
    }

    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        vkDeviceWaitIdle(m_device);
    }

    const std::vector<std::string> &resultDiagnostics() const { return m_resultDiagnostics; }

private:
    ViewerOptions m_options;
    vac::ControlProfile m_controlProfile;
    vac::ResolvedControlBindings m_bindings;
    std::optional<std::filesystem::file_time_type> m_controlProfileWriteTime;
    vac::SceneRuntime m_scene;
    vac::SceneRenderData m_renderData;
    vac::SceneDrawData m_lineData;
    std::vector<std::string> m_resultDiagnostics;
    std::optional<vac::SceneInstance> m_networkActorTemplate;
    std::vector<vac::combat::ActorState> m_actorStates;
    std::unique_ptr<vac::net::SnapshotClient> m_networkClient;
    std::unordered_map<uint8_t, size_t> m_networkActorByClientId;
    uint16_t m_networkTick = 0;
    bool m_networkTransformApplied = false;

    GLFWwindow *m_window = nullptr;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapChainImages;
    VkFormat m_swapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapChainExtent{};
    std::vector<VkImageView> m_swapChainImageViews;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_trianglePipeline = VK_NULL_HANDLE;
    VkPipeline m_linePipeline = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    Buffer m_staticTriangleBuffer;
    Buffer m_lineBuffer;
    std::unordered_map<std::string, ModelBuffer> m_modelBuffers;
    uint32_t m_staticTriangleVertexCount = 0;
    uint32_t m_lineVertexCount = 0;
    Image m_depthImage;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence> m_imagesInFlight;
    size_t m_currentFrame = 0;
    float m_sceneTimeSeconds = 0.0f;
    float m_fixedAccumulatorSeconds = 0.0f;
    float m_presentationAlpha = 1.0f;
    float m_fpsAccumulatorSeconds = 0.0f;
    uint32_t m_fpsFrames = 0;
    float m_cameraYawDegrees = 180.0f;
    float m_cameraPitchDegrees = 24.0f;
    float m_cameraDistanceWorldUnits = kDefaultCameraDistanceWorldUnits;
    bool m_cameraSteeringEnabled = false;
    bool m_cameraToggleWasDown = false;
    bool m_mouseLookInitialized = false;
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    bool m_leftMouseWasDown = false;
    std::optional<glm::vec2> m_playerMoveTarget;
    std::optional<glm::vec3> m_groundCursorPoint;
    std::vector<vac::SceneVertex> m_cursorLineVertices;
    std::optional<size_t> m_playerIndex;
    std::optional<size_t> m_sparringIndex;
    bool m_lineGeometryDirty = false;
    bool m_framebufferResized = false;

    static void framebufferResizeCallback(GLFWwindow *window, int, int)
    {
        auto *viewer = reinterpret_cast<VulkanSceneViewer *>(glfwGetWindowUserPointer(window));
        viewer->m_framebufferResized = true;
    }

    static void cursorPositionCallback(GLFWwindow *window, double x, double y)
    {
        auto *viewer = reinterpret_cast<VulkanSceneViewer *>(glfwGetWindowUserPointer(window));
        viewer->handleMouseLook(x, y);
    }

    static void scrollCallback(GLFWwindow *window, double, double yOffset)
    {
        auto *viewer = reinterpret_cast<VulkanSceneViewer *>(glfwGetWindowUserPointer(window));
        viewer->handleMouseScroll(yOffset);
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
        void *)
    {
        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            spdlog::warn("Vulkan validation: {}", callbackData->pMessage);
        }
        return VK_FALSE;
    }

    void initWindow()
    {
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("glfwInit failed");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        if (m_options.common.hiddenWindow) {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        }
        m_window = glfwCreateWindow(kWidth, kHeight, "Vulkan Action Client - Scene Viewer", nullptr, nullptr);
        if (!m_window) {
            throw std::runtime_error("glfwCreateWindow failed");
        }

        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
        glfwSetCursorPosCallback(m_window, cursorPositionCallback);
        glfwSetScrollCallback(m_window, scrollCallback);
        applyCursorMode();
    }

    void applyCursorMode()
    {
        glfwSetInputMode(m_window,
                         GLFW_CURSOR,
                         m_cameraSteeringEnabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
            glfwSetInputMode(m_window,
                             GLFW_RAW_MOUSE_MOTION,
                             m_cameraSteeringEnabled ? GLFW_TRUE : GLFW_FALSE);
        }
        m_mouseLookInitialized = false;
    }

    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDepthResources();
        createPipelines();
        createFramebuffers();
        createCommandPool();
        createVertexBuffers();
        createCommandBuffers();
        createSyncObjects();
    }

    void loadInitialControlProfile()
    {
        m_controlProfile = vac::loadControlProfile(m_options.controlProfilePath);
        m_bindings = vac::resolveControlBindings(m_controlProfile.bindings);
        m_controlProfileWriteTime = fileWriteTime(m_options.controlProfilePath);
        spdlog::info("Loaded control profile '{}'", m_options.controlProfilePath.string());
    }

    void maybeReloadControlProfile()
    {
        const std::optional<std::filesystem::file_time_type> writeTime = fileWriteTime(m_options.controlProfilePath);
        if (writeTime == m_controlProfileWriteTime) {
            return;
        }

        try {
            vac::ControlProfile nextProfile = vac::loadControlProfile(m_options.controlProfilePath);
            vac::ResolvedControlBindings nextBindings = vac::resolveControlBindings(nextProfile.bindings);
            m_controlProfile = std::move(nextProfile);
            m_bindings = std::move(nextBindings);
            m_controlProfileWriteTime = writeTime;
            applyControlProfileToActors();
            spdlog::info("Hot reloaded control profile '{}'", m_options.controlProfilePath.string());
        } catch (const std::exception &error) {
            m_controlProfileWriteTime = writeTime;
            spdlog::warn("Keeping previous control profile; reload failed: {}", error.what());
        }
    }

    void mainLoop()
    {
        uint32_t frameCount = 0;
        const auto start = std::chrono::steady_clock::now();
        auto previous = start;

        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();
            maybeReloadControlProfile();
            pumpNetworkSnapshots();
            handleCameraModeToggle();
            updateGroundCursor();
            handleClickToMoveInput();
            if (isAnyKeyPressed(m_bindings.closeViewer)) {
                glfwSetWindowShouldClose(m_window, GLFW_TRUE);
            }

            const auto now = std::chrono::steady_clock::now();
            const float deltaSeconds = std::min(std::chrono::duration<float>(now - previous).count(), 0.1f);
            previous = now;
            m_sceneTimeSeconds = std::chrono::duration<float>(now - start).count();
            updateSimulation(deltaSeconds);
            drawFrame();
            updateWindowTitle(deltaSeconds);

            if (m_options.frames > 0 && ++frameCount >= m_options.frames) {
                break;
            }

            const auto elapsed = std::chrono::steady_clock::now() - start;
            if (m_options.frames == 0 && elapsed > std::chrono::hours(24)) {
                break;
            }
        }
    }

    void findControllableActors()
    {
        for (size_t i = 0; i < m_scene.instances.size(); ++i) {
            if (m_scene.instances[i].id == "player_preview") {
                m_playerIndex = i;
            } else if (m_scene.instances[i].id == "sparring_partner") {
                m_sparringIndex = i;
            }
        }
    }

    void configureNetworkActors()
    {
        if (m_options.visualLab || m_options.common.offline) {
            return;
        }

        if (m_options.networkClientId == 0) {
            if (m_playerIndex.has_value()) {
                m_networkActorByClientId[1] = *m_playerIndex;
            }
            if (m_sparringIndex.has_value()) {
                m_networkActorByClientId[2] = *m_sparringIndex;
            }
            return;
        }

        const size_t templateIndex = networkActorTemplateIndex();
        m_networkActorTemplate = m_scene.instances[templateIndex];
        m_scene.instances.clear();
        m_playerIndex.reset();
        m_sparringIndex.reset();
        m_networkActorByClientId.clear();

        m_playerIndex = ensureNetworkActor(m_options.networkClientId);
    }

    size_t ensureNetworkActor(uint8_t clientId)
    {
        const auto actorIt = m_networkActorByClientId.find(clientId);
        if (actorIt != m_networkActorByClientId.end()) {
            return actorIt->second;
        }

        if (m_options.networkClientId != 0) {
            if (clientId == m_options.networkClientId && m_playerIndex.has_value()) {
                m_networkActorByClientId[clientId] = *m_playerIndex;
                return *m_playerIndex;
            }
            return createNetworkActorInstance(clientId);
        }

        if (clientId == 1 && m_playerIndex.has_value()) {
            m_networkActorByClientId[clientId] = *m_playerIndex;
            return *m_playerIndex;
        }
        if (clientId == 2 && m_sparringIndex.has_value()) {
            m_networkActorByClientId[clientId] = *m_sparringIndex;
            return *m_sparringIndex;
        }

        return createNetworkActorInstance(clientId);
    }

    size_t networkActorTemplateIndex() const
    {
        if (m_networkActorTemplate.has_value()) {
            return 0;
        }
        if (m_playerIndex.has_value()) {
            return *m_playerIndex;
        }
        if (m_sparringIndex.has_value()) {
            return *m_sparringIndex;
        }
        if (!m_scene.instances.empty()) {
            return 0;
        }

        throw std::runtime_error("Cannot create network actor without a model instance template");
    }

    const vac::SceneInstance &networkActorTemplate() const
    {
        if (m_networkActorTemplate.has_value()) {
            return *m_networkActorTemplate;
        }

        return m_scene.instances[networkActorTemplateIndex()];
    }

    vac::Transform networkSpawnTransform(uint8_t clientId, const vac::Transform &templateTransform) const
    {
        vac::Transform transform = templateTransform;
        const uint8_t ordinal = clientId > 2u ? static_cast<uint8_t>(clientId - 3u) : static_cast<uint8_t>(clientId - 1u);
        const uint8_t slot = static_cast<uint8_t>(ordinal % 8u);
        const uint8_t ring = static_cast<uint8_t>(ordinal / 8u);
        const float angle = glm::radians(static_cast<float>(slot) * 45.0f);
        const float radius = 18.0f + static_cast<float>(ring) * 12.0f;
        transform.translation.x = std::sin(angle) * radius;
        transform.translation.z = std::cos(angle) * radius;

        const glm::vec2 towardCenter{-transform.translation.x, -transform.translation.z};
        if (glm::dot(towardCenter, towardCenter) > 0.0001f) {
            transform.rotationDegrees.y = glm::degrees(std::atan2(towardCenter.x, towardCenter.y));
        }
        return transform;
    }

    size_t createNetworkActorInstance(uint8_t clientId)
    {
        vac::SceneInstance instance = networkActorTemplate();
        instance.id = fmt::format("network_client_{}", clientId);
        instance.name = fmt::format("Network Client {}", clientId);
        instance.transform = networkSpawnTransform(clientId, instance.transform);

        m_scene.instances.push_back(instance);
        const size_t actorIndex = m_scene.instances.size() - 1;
        m_networkActorByClientId[clientId] = actorIndex;
        vac::refreshSceneBounds(m_scene);

        if (!m_actorStates.empty()) {
            vac::combat::ActorState actor;
            actor.previousTransform = instance.transform;
            actor.currentTransform = instance.transform;
            m_actorStates.push_back(actor);
            applyControlProfileToActors();
            m_lineData = vac::buildSceneLineData(m_scene);
            m_lineGeometryDirty = true;
        }

        spdlog::info("Created network actor {} for client {}", instance.id, clientId);
        return actorIndex;
    }

    void removeNetworkActor(uint8_t clientId)
    {
        if (clientId == m_options.networkClientId) {
            return;
        }

        const auto actorIt = m_networkActorByClientId.find(clientId);
        if (actorIt == m_networkActorByClientId.end()) {
            return;
        }

        const size_t actorIndex = actorIt->second;
        m_networkActorByClientId.erase(actorIt);

        if (actorIndex < m_scene.instances.size()) {
            m_scene.instances.erase(m_scene.instances.begin() + static_cast<std::ptrdiff_t>(actorIndex));
        }
        if (actorIndex < m_actorStates.size()) {
            m_actorStates.erase(m_actorStates.begin() + static_cast<std::ptrdiff_t>(actorIndex));
        }

        for (auto &[mappedClientId, mappedActorIndex] : m_networkActorByClientId) {
            (void)mappedClientId;
            if (mappedActorIndex > actorIndex) {
                --mappedActorIndex;
            }
        }

        if (m_playerIndex.has_value() && *m_playerIndex > actorIndex) {
            --(*m_playerIndex);
        }

        vac::refreshSceneBounds(m_scene);
        m_lineData = vac::buildSceneLineData(m_scene);
        m_lineGeometryDirty = true;
        spdlog::info("Removed network actor for client {}", clientId);
    }

    void initializeNetworkClient()
    {
        if (m_options.visualLab || m_options.common.offline) {
            return;
        }

        if (m_options.networkClientId == 0) {
            return;
        }

        m_networkClient = std::make_unique<vac::net::SnapshotClient>(
            m_options.networkClientId,
            vac::net::resolveEndpoint(m_options.networkServer));
        spdlog::info("Network client {} sending snapshots to {}",
                     m_options.networkClientId,
                     m_options.networkServer);
    }

    void pumpNetworkSnapshots()
    {
        if (!m_networkClient) {
            return;
        }

        bool applied = false;
        const vac::net::SnapshotClient::ReceiveBatch batch = m_networkClient->receive();
        for (const vac::net::ServerEventPacket &event : batch.events) {
            const char *eventLabel = event.event == vac::net::ServerEventKind::clientConnected
                ? "connected"
                : "disconnected";
            spdlog::info("Network peer {} {}", event.clientId, eventLabel);
            if (event.event == vac::net::ServerEventKind::clientConnected && event.clientId != m_options.networkClientId) {
                ensureNetworkActor(event.clientId);
            } else if (event.event == vac::net::ServerEventKind::clientDisconnected) {
                removeNetworkActor(event.clientId);
            }
        }

        for (const vac::net::ActorSnapshot &snapshot : batch.snapshots) {
            const size_t actorIndex = ensureNetworkActor(snapshot.clientId);
            if (actorIndex >= m_actorStates.size()) {
                continue;
            }

            vac::combat::ActorState &actor = m_actorStates[actorIndex];
            actor.previousTransform = actor.currentTransform;
            actor.currentTransform.translation = snapshot.position;
            actor.currentTransform.rotationDegrees.y = snapshot.yawDegrees;
            applied = true;
        }

        if (applied) {
            m_networkTransformApplied = true;
            syncSceneToCombatCurrent();
        }
    }

    void sendLocalNetworkSnapshot(bool sprinting, bool moving)
    {
        if (!m_networkClient || !m_playerIndex.has_value()) {
            return;
        }

        const vac::Transform &transform = m_actorStates[*m_playerIndex].currentTransform;
        uint8_t flags = 0;
        if (m_cameraSteeringEnabled) {
            flags |= vac::net::snapshotCameraSteering;
        }
        if (sprinting) {
            flags |= vac::net::snapshotSprinting;
        }
        if (moving) {
            flags |= vac::net::snapshotMoving;
        }

        m_networkClient->sendSnapshot({
            m_options.networkClientId,
            m_networkTick++,
            flags,
            transform.translation,
            transform.rotationDegrees.y,
        });
    }

    void initializeCombatActors()
    {
        m_actorStates.resize(m_scene.instances.size());
        for (size_t i = 0; i < m_scene.instances.size(); ++i) {
            vac::combat::ActorState &actor = m_actorStates[i];
            actor.previousTransform = m_scene.instances[i].transform;
            actor.currentTransform = m_scene.instances[i].transform;
        }
        applyControlProfileToActors();
    }

    void applyControlProfileToActors()
    {
        for (size_t i = 0; i < m_actorStates.size(); ++i) {
            m_actorStates[i].moveSpeedWorldUnitsPerSecond = (m_playerIndex.has_value() && i == *m_playerIndex)
                ? m_controlProfile.movement.playerRunSpeedWorldUnitsPerSecond
                : m_controlProfile.movement.sparringMoveSpeedWorldUnitsPerSecond;
        }
    }

    void initializeCameraAnchor()
    {
        if (m_playerIndex.has_value()) {
            m_cameraYawDegrees = m_actorStates[*m_playerIndex].currentTransform.rotationDegrees.y;
        }
    }

    void handleCameraModeToggle()
    {
        const bool toggleKeyDown = isAnyKeyPressed(m_bindings.cameraModeToggle);
        if (toggleKeyDown && !m_cameraToggleWasDown) {
            setCameraSteeringEnabled(!m_cameraSteeringEnabled);
        }
        m_cameraToggleWasDown = toggleKeyDown;
    }

    void setCameraSteeringEnabled(bool enabled)
    {
        if (m_cameraSteeringEnabled == enabled) {
            return;
        }

        m_cameraSteeringEnabled = enabled;
        if (m_cameraSteeringEnabled) {
            m_playerMoveTarget.reset();
        }
        applyCursorMode();
        updateGroundCursor();
    }

    void handleMouseLook(double x, double y)
    {
        if (m_options.orbitCamera || !m_cameraSteeringEnabled) {
            return;
        }

        if (!m_mouseLookInitialized) {
            m_lastMouseX = x;
            m_lastMouseY = y;
            m_mouseLookInitialized = true;
            return;
        }

        const double dx = x - m_lastMouseX;
        const double dy = y - m_lastMouseY;
        m_lastMouseX = x;
        m_lastMouseY = y;

        constexpr float sensitivity = 0.12f;
        m_cameraYawDegrees -= static_cast<float>(dx) * sensitivity;
        m_cameraPitchDegrees = std::clamp(m_cameraPitchDegrees - static_cast<float>(dy) * sensitivity,
                                          8.0f,
                                          58.0f);
    }

    void handleMouseScroll(double yOffset)
    {
        if (yOffset == 0.0) {
            return;
        }

        m_cameraDistanceWorldUnits = std::clamp(
            m_cameraDistanceWorldUnits - static_cast<float>(yOffset) * kCameraScrollZoomStepWorldUnits,
            kMinCameraDistanceWorldUnits,
            kMaxCameraDistanceWorldUnits);
    }

    void handleClickToMoveInput()
    {
        const bool leftMouseDown = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (!m_cameraSteeringEnabled && leftMouseDown && !m_leftMouseWasDown && m_groundCursorPoint.has_value()) {
            m_playerMoveTarget = glm::vec2{m_groundCursorPoint->x, m_groundCursorPoint->z};
            rebuildCursorLineVertices();
        }
        m_leftMouseWasDown = leftMouseDown;
    }

    void updateGroundCursor()
    {
        if (m_cameraSteeringEnabled) {
            const bool hadCursorLines = !m_cursorLineVertices.empty() || m_groundCursorPoint.has_value();
            m_groundCursorPoint.reset();
            m_cursorLineVertices.clear();
            if (hadCursorLines) {
                m_lineGeometryDirty = true;
            }
            return;
        }

        m_groundCursorPoint = groundPointUnderCursor();
        rebuildCursorLineVertices();
    }

    std::optional<glm::vec3> groundPointUnderCursor() const
    {
        if (m_swapChainExtent.width == 0 || m_swapChainExtent.height == 0) {
            return std::nullopt;
        }

        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(m_window, &cursorX, &cursorY);

        const float width = static_cast<float>(m_swapChainExtent.width);
        const float height = static_cast<float>(m_swapChainExtent.height);
        const float ndcX = static_cast<float>((cursorX / width) * 2.0 - 1.0);
        const float ndcY = static_cast<float>(1.0 - (cursorY / height) * 2.0);

        const CameraView camera = cameraView();
        const glm::vec3 viewForward = glm::normalize(camera.target - camera.eye);
        const glm::vec3 viewRight = glm::normalize(glm::cross(viewForward, glm::vec3{0.0f, 1.0f, 0.0f}));
        const glm::vec3 viewUp = glm::normalize(glm::cross(viewRight, viewForward));
        const float verticalScale = std::tan(glm::radians(50.0f) * 0.5f);
        const float aspect = width / height;
        const glm::vec3 rayDirection = glm::normalize(viewForward +
                                                      viewRight * ndcX * verticalScale * aspect +
                                                      viewUp * ndcY * verticalScale);

        constexpr float groundY = 0.04f;
        if (std::abs(rayDirection.y) < 0.0001f) {
            return std::nullopt;
        }

        const float distance = (groundY - camera.eye.y) / rayDirection.y;
        if (distance <= 0.0f) {
            return std::nullopt;
        }

        glm::vec3 point = camera.eye + rayDirection * distance;
        const vac::combat::ArenaLimits arena = arenaLimits();
        const glm::vec2 allowed = glm::max(arena.halfExtents - glm::vec2{arena.edgeInset}, glm::vec2{0.0f});
        point.x = std::clamp(point.x, -allowed.x, allowed.x);
        point.y = groundY;
        point.z = std::clamp(point.z, -allowed.y, allowed.y);
        return point;
    }

    void rebuildCursorLineVertices()
    {
        m_cursorLineVertices.clear();
        if (m_groundCursorPoint.has_value()) {
            appendGroundSquare(m_cursorLineVertices, *m_groundCursorPoint, 1.4f, {0.28f, 0.88f, 1.0f});
        }
        if (m_playerMoveTarget.has_value()) {
            appendGroundSquare(m_cursorLineVertices,
                               {m_playerMoveTarget->x, 0.06f, m_playerMoveTarget->y},
                               2.0f,
                               {1.0f, 0.82f, 0.24f});
        }
        m_lineGeometryDirty = true;
    }

    static void appendGroundSquare(std::vector<vac::SceneVertex> &vertices,
                                   glm::vec3 center,
                                   float halfSize,
                                   glm::vec3 color)
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

    static void appendLine(std::vector<vac::SceneVertex> &vertices, glm::vec3 a, glm::vec3 b, glm::vec3 color)
    {
        vertices.push_back({a, {0.0f, 1.0f, 0.0f}, color});
        vertices.push_back({b, {0.0f, 1.0f, 0.0f}, color});
    }

    void updateSimulation(float deltaSeconds)
    {
        m_fixedAccumulatorSeconds += std::min(deltaSeconds, 0.25f);

        const vac::combat::LocalMoveIntent playerIntent = readPlayerMoveAxes();
        const bool lockPlayerFacingToCamera = shouldLockPlayerFacingToCamera();
        const bool playerBackpedaling = playerIntent.axes.y < -0.0001f;
        const bool playerSprinting = isPlayerSprintPressed() && !playerBackpedaling;
        const bool hasPlayerMoveInput = glm::dot(playerIntent.axes, playerIntent.axes) > 0.0001f;
        if (hasPlayerMoveInput && m_playerMoveTarget.has_value()) {
            m_playerMoveTarget.reset();
            rebuildCursorLineVertices();
        }
        float sparringYawDegrees = 0.0f;
        if (m_sparringIndex.has_value()) {
            sparringYawDegrees = m_actorStates[*m_sparringIndex].currentTransform.rotationDegrees.y;
        }
        const vac::combat::LocalMoveIntent sparringIntent =
            readKeyboardAxes(m_bindings.sparringMoveLeft,
                             m_bindings.sparringMoveRight,
                             m_bindings.sparringMoveForward,
                             m_bindings.sparringMoveBackward);
        const vac::combat::ArenaLimits arena = arenaLimits();

        int ticks = 0;
        bool moved = false;
        while (m_fixedAccumulatorSeconds >= vac::combat::kFixedTickSeconds &&
               ticks < vac::combat::kMaxCatchUpTicksPerFrame) {
            for (vac::combat::ActorState &actor : m_actorStates) {
                vac::combat::beginTick(actor);
            }

            if (m_playerIndex.has_value()) {
                vac::combat::ActorState &player = m_actorStates[*m_playerIndex];
                player.moveSpeedWorldUnitsPerSecond =
                    m_controlProfile.movement.playerRunSpeedWorldUnitsPerSecond *
                    (playerSprinting ? m_controlProfile.movement.playerSprintSpeedScale : 1.0f);
                if (!lockPlayerFacingToCamera && !hasPlayerMoveInput && m_playerMoveTarget.has_value()) {
                    moved |= vac::combat::applyMoveToWorldTarget(player,
                                                                 *m_playerMoveTarget,
                                                                 vac::combat::kFixedTickSeconds,
                                                                 arena,
                                                                 m_controlProfile.movement);
                    if (isAtMoveTarget(player, *m_playerMoveTarget)) {
                        m_playerMoveTarget.reset();
                        rebuildCursorLineVertices();
                    }
                } else {
                    vac::combat::LocalMoveIntent tickIntent = playerIntent;
                    tickIntent.axes.x *= -1.0f;
                    const float movementYawDegrees = lockPlayerFacingToCamera
                        ? m_cameraYawDegrees
                        : player.currentTransform.rotationDegrees.y;

                    moved |= vac::combat::applyFramedStrafeLocomotion(player,
                                                                      tickIntent,
                                                                      {movementYawDegrees},
                                                                      lockPlayerFacingToCamera,
                                                                      vac::combat::kFixedTickSeconds,
                                                                      arena,
                                                                      m_controlProfile.movement);
                }
                sendLocalNetworkSnapshot(playerSprinting, hasPlayerMoveInput || m_playerMoveTarget.has_value());
            }

            if (m_sparringIndex.has_value() && !m_networkClient) {
                m_actorStates[*m_sparringIndex].moveSpeedWorldUnitsPerSecond =
                    m_controlProfile.movement.sparringMoveSpeedWorldUnitsPerSecond;
                moved |= vac::combat::applyCharacterLocomotion(m_actorStates[*m_sparringIndex],
                                                               sparringIntent,
                                                               {sparringYawDegrees},
                                                               vac::combat::kFixedTickSeconds,
                                                               arena,
                                                               m_controlProfile.movement);
            }

            m_fixedAccumulatorSeconds -= vac::combat::kFixedTickSeconds;
            ++ticks;
        }

        if (ticks == vac::combat::kMaxCatchUpTicksPerFrame &&
            m_fixedAccumulatorSeconds >= vac::combat::kFixedTickSeconds) {
            m_fixedAccumulatorSeconds = 0.0f;
        }

        m_presentationAlpha = std::clamp(m_fixedAccumulatorSeconds / vac::combat::kFixedTickSeconds, 0.0f, 1.0f);

        moved = moved || m_networkTransformApplied;
        m_networkTransformApplied = false;

        if (ticks > 0 && moved) {
            syncSceneToCombatCurrent();
        }
    }

    vac::combat::ArenaLimits arenaLimits() const
    {
        for (const vac::ProceduralInstance &instance : m_scene.procedural) {
            if (instance.type == "floor") {
                return {
                    instance.size * 0.5f,
                    m_controlProfile.movement.arenaEdgeInsetWorldUnits,
                };
            }
        }

        return {};
    }

    void syncSceneToCombatCurrent()
    {
        for (size_t i = 0; i < m_scene.instances.size() && i < m_actorStates.size(); ++i) {
            m_scene.instances[i].transform = m_actorStates[i].currentTransform;
        }

        vac::refreshSceneBounds(m_scene);
        m_lineData = vac::buildSceneLineData(m_scene);
        m_lineGeometryDirty = true;
    }

    vac::Transform presentationTransform(size_t actorIndex) const
    {
        if (actorIndex >= m_actorStates.size()) {
            return m_scene.instances[actorIndex].transform;
        }

        return vac::combat::interpolate(m_actorStates[actorIndex], m_presentationAlpha);
    }

    bool shouldLockPlayerFacingToCamera() const
    {
        return m_cameraSteeringEnabled;
    }

    bool isPlayerSprintPressed() const
    {
        return isAnyKeyPressed(m_bindings.playerSprint);
    }

    bool isAnyKeyPressed(const std::vector<int> &keys) const
    {
        for (const int key : keys) {
            if (glfwGetKey(m_window, key) == GLFW_PRESS) {
                return true;
            }
        }
        return false;
    }

    bool isAtMoveTarget(const vac::combat::ActorState &actor, glm::vec2 target) const
    {
        const glm::vec2 current{actor.currentTransform.translation.x, actor.currentTransform.translation.z};
        const glm::vec2 delta = target - current;
        const float radius = m_controlProfile.movement.moveTargetArrivalRadiusWorldUnits;
        return glm::dot(delta, delta) <= radius * radius;
    }

    vac::combat::LocalMoveIntent readPlayerMoveAxes() const
    {
        glm::vec2 axes = readKeyboardAxes(m_bindings.playerMoveLeft,
                                          m_bindings.playerMoveRight,
                                          m_bindings.playerMoveForward,
                                          m_bindings.playerMoveBackward)
                             .axes;

#ifdef _WIN32
        XINPUT_STATE state{};
        if (XInputGetState(0, &state) == ERROR_SUCCESS) {
            const glm::vec2 stick = normalizedThumbstick(state.Gamepad.sThumbLX,
                                                        state.Gamepad.sThumbLY,
                                                        XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
            if (glm::dot(stick, stick) > 0.0f) {
                axes += stick;
            }
        }
#endif

        return {normalizedMove(axes)};
    }

    vac::combat::LocalMoveIntent readKeyboardAxes(const std::vector<int> &leftKeys,
                                                  const std::vector<int> &rightKeys,
                                                  const std::vector<int> &forwardKeys,
                                                  const std::vector<int> &backwardKeys) const
    {
        glm::vec2 axes{0.0f};
        if (isAnyKeyPressed(leftKeys)) {
            axes.x -= 1.0f;
        }
        if (isAnyKeyPressed(rightKeys)) {
            axes.x += 1.0f;
        }
        if (isAnyKeyPressed(forwardKeys)) {
            axes.y += 1.0f;
        }
        if (isAnyKeyPressed(backwardKeys)) {
            axes.y -= 1.0f;
        }
        return {normalizedMove(axes)};
    }

    static glm::vec2 normalizedMove(glm::vec2 move)
    {
        const float lengthSquared = glm::dot(move, move);
        if (lengthSquared > 1.0f) {
            return move / std::sqrt(lengthSquared);
        }
        return move;
    }

#ifdef _WIN32
    static glm::vec2 normalizedThumbstick(SHORT rawX, SHORT rawY, SHORT deadzone)
    {
        const glm::vec2 value{static_cast<float>(rawX), static_cast<float>(rawY)};
        const float magnitude = std::sqrt(glm::dot(value, value));
        if (magnitude <= static_cast<float>(deadzone)) {
            return {0.0f, 0.0f};
        }

        const float normalizedMagnitude = std::min((magnitude - static_cast<float>(deadzone)) /
                                                       (32767.0f - static_cast<float>(deadzone)),
                                                   1.0f);
        return (value / magnitude) * normalizedMagnitude;
    }
#endif

    void updateWindowTitle(float deltaSeconds)
    {
        m_fpsAccumulatorSeconds += deltaSeconds;
        ++m_fpsFrames;

        if (m_fpsAccumulatorSeconds < 0.5f) {
            return;
        }

        const float fps = static_cast<float>(m_fpsFrames) / m_fpsAccumulatorSeconds;
        glm::vec3 playerPosition{0.0f};
        if (m_playerIndex.has_value()) {
            playerPosition = presentationTransform(*m_playerIndex).translation;
        }

        const std::string networkLabel = m_networkClient
            ? fmt::format("net{}", m_options.networkClientId)
            : "local";
        const std::string title = fmt::format("Vulkan Action Client - {:.0f} FPS | {} | mode={} | player x={:.1f} z={:.1f}",
                                             fps,
                                             networkLabel,
                                             m_cameraSteeringEnabled ? "camera" : "cursor",
                                             playerPosition.x,
                                             playerPosition.z);
        glfwSetWindowTitle(m_window, title.c_str());
        m_fpsAccumulatorSeconds = 0.0f;
        m_fpsFrames = 0;
    }

    void cleanupSwapChain()
    {
        destroyDepthResources();

        for (VkFramebuffer framebuffer : m_framebuffers) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
        m_framebuffers.clear();

        for (VkImageView imageView : m_swapChainImageViews) {
            vkDestroyImageView(m_device, imageView, nullptr);
        }
        m_swapChainImageViews.clear();

        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
        m_swapChain = VK_NULL_HANDLE;
    }

    void cleanup()
    {
        if (m_device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device);

            cleanupSwapChain();

            destroyBuffer(m_staticTriangleBuffer);
            destroyBuffer(m_lineBuffer);
            for (auto &[_, modelBuffer] : m_modelBuffers) {
                destroyBuffer(modelBuffer.buffer);
            }
            m_modelBuffers.clear();

            vkDestroyPipeline(m_device, m_trianglePipeline, nullptr);
            vkDestroyPipeline(m_device, m_linePipeline, nullptr);
            vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
            vkDestroyRenderPass(m_device, m_renderPass, nullptr);

            destroySyncObjects();

            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            vkDestroyDevice(m_device, nullptr);
        }

        if (m_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        }

        if (m_debugMessenger != VK_NULL_HANDLE) {
            auto destroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyDebugUtilsMessenger) {
                destroyDebugUtilsMessenger(m_instance, m_debugMessenger, nullptr);
            }
        }

        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
        }

        if (m_window) {
            glfwDestroyWindow(m_window);
            glfwTerminate();
        }
    }

    void recreateSwapChain()
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(m_device);
        destroySyncObjects();
        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createDepthResources();
        createFramebuffers();
        createCommandBuffers();
        createSyncObjects();
    }

    void createInstance()
    {
        if (kEnableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested but unavailable");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vulkan Action Client";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.pEngineName = "vac";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        const std::vector<const char *> extensions = requiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (kEnableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
            createInfo.ppEnabledLayerNames = kValidationLayers.data();
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = &debugCreateInfo;
        }

        if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateInstance failed");
        }
    }

    bool checkValidationLayerSupport()
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName : kValidationLayers) {
            const bool found = std::any_of(availableLayers.begin(), availableLayers.end(), [&](const VkLayerProperties &layer) {
                return std::strcmp(layerName, layer.layerName) == 0;
            });
            if (!found) {
                return false;
            }
        }
        return true;
    }

    std::vector<const char *> requiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (kEnableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
    {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger()
    {
        if (!kEnableValidationLayers) {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        populateDebugMessengerCreateInfo(createInfo);

        auto createDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        if (createDebugUtilsMessenger &&
            createDebugUtilsMessenger(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateDebugUtilsMessengerEXT failed");
        }
    }

    void createSurface()
    {
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
            throw std::runtime_error("glfwCreateWindowSurface failed");
        }
    }

    void pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("No Vulkan physical devices found");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        for (VkPhysicalDevice device : devices) {
            if (isDeviceSuitable(device)) {
                m_physicalDevice = device;
                break;
            }
        }

        if (m_physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("No suitable Vulkan physical device found");
        }
    }

    bool isDeviceSuitable(VkPhysicalDevice device)
    {
        const QueueFamilyIndices indices = findQueueFamilies(device);
        const bool extensionsSupported = checkDeviceExtensionSupport(device);
        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.complete() && extensionsSupported && swapChainAdequate;
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.complete()) {
                break;
            }
        }

        return indices;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        for (const VkExtensionProperties &extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }
        return requiredExtensions.empty();
    }

    void createLogicalDevice()
    {
        const QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        const std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value(),
        };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;

        const std::array<const char *, 1> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateDevice failed");
        }

        vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats)
    {
        for (const VkSurfaceFormatKHR &format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }
        return formats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &presentModes)
    {
        for (const VkPresentModeKHR &presentMode : presentModes) {
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return presentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);

        VkExtent2D extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
        };

        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return extent;
    }

    void createSwapChain()
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);
        const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        const VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        const VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        const QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
        const uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateSwapchainKHR failed");
        }

        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
        m_swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

        m_swapChainImageFormat = surfaceFormat.format;
        m_swapChainExtent = extent;
    }

    void createImageViews()
    {
        m_swapChainImageViews.resize(m_swapChainImages.size());
        for (size_t i = 0; i < m_swapChainImages.size(); ++i) {
            m_swapChainImageViews[i] = createImageView(m_swapChainImages[i], m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView = VK_NULL_HANDLE;
        if (vkCreateImageView(m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateImageView failed");
        }
        return imageView;
    }

    void createRenderPass()
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = findDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        const std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateRenderPass failed");
        }
    }

    void createPipelines()
    {
        const std::vector<char> vertShaderCode = readFile(VULKAN_ACTION_CLIENT_SCENE_VERT_SPV);
        const std::vector<char> fragShaderCode = readFile(VULKAN_ACTION_CLIENT_SCENE_FRAG_SPV);
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        const VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        const auto bindingDescription = vertexBindingDescription();
        const auto attributeDescriptions = vertexAttributeDescriptions();
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_swapChainExtent.width);
        viewport.height = static_cast<float>(m_swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_swapChainExtent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("vkCreatePipelineLayout failed");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;

        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_trianglePipeline) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateGraphicsPipelines triangle failed");
        }

        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        depthStencil.depthWriteEnable = VK_FALSE;
        if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_linePipeline) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateGraphicsPipelines line failed");
        }

        vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    }

    VkShaderModule createShaderModule(const std::vector<char> &code)
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateShaderModule failed");
        }
        return shaderModule;
    }

    void createFramebuffers()
    {
        m_framebuffers.resize(m_swapChainImageViews.size());
        for (size_t i = 0; i < m_swapChainImageViews.size(); ++i) {
            const std::array<VkImageView, 2> attachments = {
                m_swapChainImageViews[i],
                m_depthImage.view,
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = m_swapChainExtent.width;
            framebufferInfo.height = m_swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("vkCreateFramebuffer failed");
            }
        }
    }

    void createCommandPool()
    {
        const QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateCommandPool failed");
        }
    }

    void createVertexBuffers()
    {
        m_staticTriangleVertexCount = static_cast<uint32_t>(m_renderData.staticTriangleVertices.size());
        createVertexBuffer(m_renderData.staticTriangleVertices, m_staticTriangleBuffer);
        const std::vector<vac::SceneVertex> lineVertices = combinedLineVertices();
        m_lineVertexCount = static_cast<uint32_t>(lineVertices.size());
        createVertexBuffer(lineVertices, m_lineBuffer);

        size_t modelVertexCount = 0;
        for (const auto &[modelId, vertices] : m_renderData.modelVerticesById) {
            ModelBuffer modelBuffer;
            modelBuffer.vertexCount = static_cast<uint32_t>(vertices.size());
            createVertexBuffer(vertices, modelBuffer.buffer);
            modelVertexCount += vertices.size();
            m_modelBuffers.emplace(modelId, std::move(modelBuffer));
        }

        spdlog::info("Scene viewer vertices: static_triangles={} model_vertices={} lines={}",
                     m_renderData.staticTriangleVertices.size(),
                     modelVertexCount,
                     m_lineVertexCount);
    }

    std::vector<vac::SceneVertex> combinedLineVertices() const
    {
        std::vector<vac::SceneVertex> vertices;
        vertices.reserve(m_renderData.staticLineVertices.size() +
                         m_lineData.lineVertices.size() +
                         m_cursorLineVertices.size());
        vertices.insert(vertices.end(), m_renderData.staticLineVertices.begin(), m_renderData.staticLineVertices.end());
        vertices.insert(vertices.end(), m_lineData.lineVertices.begin(), m_lineData.lineVertices.end());
        vertices.insert(vertices.end(), m_cursorLineVertices.begin(), m_cursorLineVertices.end());
        return vertices;
    }

    void createVertexBuffer(const std::vector<vac::SceneVertex> &vertices, Buffer &buffer)
    {
        if (vertices.empty()) {
            return;
        }

        const VkDeviceSize bufferSize = sizeof(vac::SceneVertex) * vertices.size();
        createBuffer(bufferSize,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     buffer);
        buffer.size = bufferSize;

        uploadVertexBuffer(vertices, buffer);
    }

    void uploadVertexBuffer(const std::vector<vac::SceneVertex> &vertices, Buffer &buffer)
    {
        if (vertices.empty()) {
            return;
        }

        const VkDeviceSize bufferSize = sizeof(vac::SceneVertex) * vertices.size();
        if (buffer.buffer == VK_NULL_HANDLE || bufferSize > buffer.size) {
            destroyBuffer(buffer);
            createVertexBuffer(vertices, buffer);
            return;
        }

        void *data = nullptr;
        vkMapMemory(m_device, buffer.memory, 0, bufferSize, 0, &data);
        std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(m_device, buffer.memory);
    }

    void createCommandBuffers()
    {
        if (!m_commandBuffers.empty()) {
            vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
        }

        m_commandBuffers.resize(m_framebuffers.size());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

        if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("vkAllocateCommandBuffers failed");
        }
    }

    CameraView cameraView() const
    {
        CameraView camera;
        camera.worldRadius = m_scene.worldBounds.valid
            ? std::max({m_scene.worldBounds.max.x - m_scene.worldBounds.min.x,
                        m_scene.worldBounds.max.y - m_scene.worldBounds.min.y,
                        m_scene.worldBounds.max.z - m_scene.worldBounds.min.z,
                        18.0f}) * 0.5f
            : 32.0f;

        vac::Transform anchorTransform;
        if (m_playerIndex.has_value()) {
            anchorTransform = presentationTransform(*m_playerIndex);
        }

        const float yaw = glm::radians(m_cameraYawDegrees);
        const float pitch = glm::radians(m_cameraPitchDegrees);
        const glm::vec3 forward{std::sin(yaw), 0.0f, std::cos(yaw)};
        const glm::vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
        const glm::vec3 anchor = anchorTransform.translation + glm::vec3{0.0f, 6.5f, 0.0f};
        const float cameraDistance = m_cameraDistanceWorldUnits;
        const float shoulderBlend = std::clamp(cameraDistance / kCameraFirstPersonBlendDistanceWorldUnits,
                                               0.0f,
                                               1.0f);
        glm::vec3 cameraOffset = -forward * (std::cos(pitch) * cameraDistance) +
                                 right * (5.0f * shoulderBlend) +
                                 glm::vec3{0.0f, std::sin(pitch) * cameraDistance, 0.0f};

        if (m_options.orbitCamera) {
            const float orbitRadians = m_sceneTimeSeconds * 0.22f;
            const glm::mat4 orbit = glm::rotate(glm::mat4{1.0f}, orbitRadians, glm::vec3{0.0f, 1.0f, 0.0f});
            cameraOffset = glm::vec3{orbit * glm::vec4{cameraOffset, 0.0f}};
        }

        camera.eye = anchor + cameraOffset;
        camera.target = anchor + forward * 10.0f + glm::vec3{0.0f, 2.0f * shoulderBlend, 0.0f};
        return camera;
    }

    glm::mat4 viewProjection() const
    {
        const CameraView camera = cameraView();
        glm::mat4 view = glm::lookAt(camera.eye, camera.target, glm::vec3{0.0f, 1.0f, 0.0f});
        glm::mat4 projection = glm::perspective(glm::radians(50.0f),
                                                static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height),
                                                0.1f,
                                                std::max(100.0f, camera.worldRadius * 6.0f));
        projection[1][1] *= -1.0f;
        return projection * view;
    }

    void pushDrawConstants(VkCommandBuffer commandBuffer,
                           const glm::mat4 &viewProjection,
                           const glm::mat4 &model,
                           glm::vec3 color)
    {
        const PushConstants pushConstants{
            viewProjection * model,
            glm::vec4{color, 1.0f},
        };
        vkCmdPushConstants(commandBuffer,
                           m_pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(PushConstants),
                           &pushConstants);
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("vkBeginCommandBuffer failed");
        }

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.055f, 0.062f, 0.070f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass;
        renderPassInfo.framebuffer = m_framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_swapChainExtent;
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        const glm::mat4 vp = viewProjection();

        VkDeviceSize offsets[] = {0};
        if (m_staticTriangleVertexCount > 0) {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_trianglePipeline);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_staticTriangleBuffer.buffer, offsets);
            pushDrawConstants(commandBuffer, vp, glm::mat4{1.0f}, {1.0f, 1.0f, 1.0f});
            vkCmdDraw(commandBuffer, m_staticTriangleVertexCount, 1, 0, 0);
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_trianglePipeline);
        for (size_t instanceIndex = 0; instanceIndex < m_scene.instances.size(); ++instanceIndex) {
            const vac::SceneInstance &instance = m_scene.instances[instanceIndex];
            const auto bufferIt = m_modelBuffers.find(instance.modelId);
            if (bufferIt == m_modelBuffers.end() || bufferIt->second.vertexCount == 0) {
                continue;
            }

            const vac::Transform transform = presentationTransform(instanceIndex);
            const glm::mat4 model = vac::makeTransformMatrix(transform);
            pushDrawConstants(commandBuffer, vp, model, vac::instanceColor(instanceIndex));
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &bufferIt->second.buffer.buffer, offsets);
            vkCmdDraw(commandBuffer, bufferIt->second.vertexCount, 1, 0, 0);
        }

        if (m_lineVertexCount > 0) {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_lineBuffer.buffer, offsets);
            pushDrawConstants(commandBuffer, vp, glm::mat4{1.0f}, {1.0f, 1.0f, 1.0f});
            vkCmdDraw(commandBuffer, m_lineVertexCount, 1, 0, 0);
        }

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("vkEndCommandBuffer failed");
        }
    }

    void createSyncObjects()
    {
        m_imageAvailableSemaphores.assign(kMaxFramesInFlight, VK_NULL_HANDLE);
        m_renderFinishedSemaphores.assign(m_swapChainImages.size(), VK_NULL_HANDLE);
        m_inFlightFences.assign(kMaxFramesInFlight, VK_NULL_HANDLE);
        m_imagesInFlight.assign(m_swapChainImages.size(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("createSyncObjects failed");
            }
        }

        for (size_t i = 0; i < m_renderFinishedSemaphores.size(); ++i) {
            if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS) {
                throw std::runtime_error("createSyncObjects failed");
            }
        }
    }

    void destroySyncObjects()
    {
        for (VkSemaphore semaphore : m_renderFinishedSemaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device, semaphore, nullptr);
            }
        }
        m_renderFinishedSemaphores.clear();

        for (VkSemaphore semaphore : m_imageAvailableSemaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device, semaphore, nullptr);
            }
        }
        m_imageAvailableSemaphores.clear();

        for (VkFence fence : m_inFlightFences) {
            if (fence != VK_NULL_HANDLE) {
                vkDestroyFence(m_device, fence, nullptr);
            }
        }
        m_inFlightFences.clear();
        m_imagesInFlight.clear();
        m_currentFrame = 0;
    }

    void drawFrame()
    {
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

        if (m_lineGeometryDirty) {
            const std::vector<vac::SceneVertex> lineVertices = combinedLineVertices();
            m_lineVertexCount = static_cast<uint32_t>(lineVertices.size());
            uploadVertexBuffer(lineVertices, m_lineBuffer);
            m_lineGeometryDirty = false;
        }

        uint32_t imageIndex = 0;
        VkResult result = vkAcquireNextImageKHR(m_device,
                                                m_swapChain,
                                                UINT64_MAX,
                                                m_imageAvailableSemaphores[m_currentFrame],
                                                VK_NULL_HANDLE,
                                                &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("vkAcquireNextImageKHR failed");
        }

        if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(m_device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];

        vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
        vkResetCommandBuffer(m_commandBuffers[imageIndex], 0);
        recordCommandBuffer(m_commandBuffers[imageIndex], imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];

        VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[imageIndex]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("vkQueueSubmit failed");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {m_swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
            m_framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("vkQueuePresentKHR failed");
        }

        m_currentFrame = (m_currentFrame + 1) % kMaxFramesInFlight;
    }

    void createDepthResources()
    {
        const VkFormat depthFormat = findDepthFormat();
        createImage(m_swapChainExtent.width,
                    m_swapChainExtent.height,
                    depthFormat,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    m_depthImage);
        m_depthImage.view = createImageView(m_depthImage.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    void destroyDepthResources()
    {
        if (m_depthImage.view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_depthImage.view, nullptr);
            m_depthImage.view = VK_NULL_HANDLE;
        }
        if (m_depthImage.image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_depthImage.image, nullptr);
            m_depthImage.image = VK_NULL_HANDLE;
        }
        if (m_depthImage.memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_depthImage.memory, nullptr);
            m_depthImage.memory = VK_NULL_HANDLE;
        }
    }

    VkFormat findDepthFormat()
    {
        return findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                                   VK_IMAGE_TILING_OPTIMAL,
                                   VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates,
                                 VkImageTiling tiling,
                                 VkFormatFeatureFlags features)
    {
        for (VkFormat format : candidates) {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            }
            if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("No supported depth format found");
    }

    void createImage(uint32_t width,
                     uint32_t height,
                     VkFormat format,
                     VkImageTiling tiling,
                     VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties,
                     Image &image)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_device, &imageInfo, nullptr, &image.image) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateImage failed");
        }

        VkMemoryRequirements memRequirements{};
        vkGetImageMemoryRequirements(m_device, image.image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &image.memory) != VK_SUCCESS) {
            throw std::runtime_error("vkAllocateMemory image failed");
        }

        vkBindImageMemory(m_device, image.image, image.memory, 0);
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer &buffer)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer.buffer) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateBuffer failed");
        }

        VkMemoryRequirements memRequirements{};
        vkGetBufferMemoryRequirements(m_device, buffer.buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &buffer.memory) != VK_SUCCESS) {
            throw std::runtime_error("vkAllocateMemory buffer failed");
        }

        vkBindBufferMemory(m_device, buffer.buffer, buffer.memory, 0);
    }

    void destroyBuffer(Buffer &buffer)
    {
        if (buffer.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, buffer.buffer, nullptr);
            buffer.buffer = VK_NULL_HANDLE;
        }
        if (buffer.memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, buffer.memory, nullptr);
            buffer.memory = VK_NULL_HANDLE;
        }
        buffer.size = 0;
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties{};
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type");
    }
};
} // namespace

int main(int argc, char **argv)
{
    std::optional<std::filesystem::path> resultFile;
    try {
        const ViewerOptions options = parseOptions(argc, argv);
        resultFile = options.common.resultFile;
        VulkanSceneViewer viewer{options};
        viewer.run();
        if (resultFile.has_value()) {
            vac::host::HostResult result = vac::host::resultFromOptions("vulkan_scene_viewer", options.common);
            result.message = options.visualLab
                ? fmt::format("Visual lab rendered {} frame(s)", options.frames)
                : options.frames > 0
                ? fmt::format("Rendered {} frame(s)", options.frames)
                : "Viewer exited normally";
            result.diagnostics = viewer.resultDiagnostics();
            vac::host::writeResultFile(*resultFile, result);
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "vulkan_scene_viewer failed: " << error.what() << "\n";
        if (resultFile.has_value()) {
            try {
                vac::host::HostResult result;
                result.host = "vulkan_scene_viewer";
                result.status = "error";
                result.message = error.what();
                vac::host::writeResultFile(*resultFile, result);
            } catch (const std::exception &resultError) {
                std::cerr << "vulkan_scene_viewer could not write result file: " << resultError.what() << "\n";
            }
        }
        return 1;
    }
}
