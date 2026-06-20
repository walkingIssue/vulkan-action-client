#include "host/host_cli.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace
{
struct RequiredAsset
{
    std::filesystem::path relativePath;
    std::string description;
    bool directory = false;
};

std::filesystem::path projectRoot()
{
#ifdef VULKAN_ACTION_CLIENT_PROJECT_ROOT
    return std::filesystem::path{VULKAN_ACTION_CLIENT_PROJECT_ROOT};
#else
    return std::filesystem::current_path();
#endif
}

std::vector<RequiredAsset> requiredAssets()
{
    return {
        {
            "assets/extracted/stylized_paladin_obj/Stylized_Paladin_Clean.obj",
            "legacy bootstrap paladin OBJ used by characterization_tests and viewer_smoke",
            false,
        },
        {
            "assets/extracted/stylized_paladin_obj/Stylized_Paladin_Clean.mtl",
            "legacy bootstrap paladin material file used by Assimp import",
            false,
        },
        {
            "assets/extracted/stylized_paladin_obj/Textures",
            "legacy bootstrap paladin texture directory referenced by the material file",
            true,
        },
    };
}

bool hasDirectoryEntries(const std::filesystem::path &path)
{
    std::error_code error;
    const std::filesystem::directory_iterator begin{path, error};
    if (error) {
        return false;
    }
    return begin != std::filesystem::directory_iterator{};
}

bool assetPresent(const std::filesystem::path &path, bool directory)
{
    std::error_code error;
    if (directory) {
        return std::filesystem::is_directory(path, error) && hasDirectoryEntries(path);
    }
    if (!std::filesystem::is_regular_file(path, error)) {
        return false;
    }
    error.clear();
    const uintmax_t size = std::filesystem::file_size(path, error);
    return !error && size > 0;
}

std::vector<std::string> checkAssets(const std::filesystem::path &root, uint32_t &missingCount)
{
    std::vector<std::string> diagnostics;
    missingCount = 0;

    for (const RequiredAsset &asset : requiredAssets()) {
        const std::filesystem::path absolutePath = root / asset.relativePath;
        if (assetPresent(absolutePath, asset.directory)) {
            diagnostics.push_back("present: " + asset.relativePath.generic_string() + " - " + asset.description);
            continue;
        }

        ++missingCount;
        diagnostics.push_back("missing: " + asset.relativePath.generic_string() + " - " + asset.description);
    }

    if (missingCount > 0) {
        diagnostics.push_back(
            "remediation: copy the ignored assets/extracted directory from "
            "C:/Users/Bartek/Documents/Playground/vulkan-action-client/assets/extracted when available; "
            "do not commit extracted asset payloads");
        diagnostics.push_back(
            "note: visual_lab_smoke renders Sprint 1 primitive content and does not require these imported paladin assets");
    }

    return diagnostics;
}

void writeResult(const std::optional<std::filesystem::path> &resultFile, const vac::host::HostResult &result)
{
    if (resultFile.has_value()) {
        vac::host::writeResultFile(*resultFile, result);
    }
}
} // namespace

int main(int argc, char **argv)
{
    std::optional<std::filesystem::path> resultFile;
    try {
        vac::host::CommandLine commandLine{argc, argv};
        const vac::host::CommonHostOptions commonOptions = vac::host::parseCommonOptions(commandLine);
        resultFile = commonOptions.resultFile;
        vac::host::rejectUnsupportedCommonOptions(commonOptions, {"--result-file", "--headless"});

        std::filesystem::path root = projectRoot();
        if (const std::optional<std::string> rootValue = commandLine.consumeValue("--root")) {
            root = *rootValue;
        }
        commandLine.rejectUnknown();

        uint32_t missingCount = 0;
        vac::host::HostResult result = vac::host::resultFromOptions("asset_fixture_check", commonOptions);
        result.diagnostics = checkAssets(root, missingCount);
        result.status = missingCount == 0 ? "ok" : "error";
        result.message = missingCount == 0
            ? "All local asset fixtures are present"
            : "Missing local asset fixtures required by legacy viewer/characterization tests";
        writeResult(resultFile, result);

        for (const std::string &diagnostic : result.diagnostics) {
            std::cout << diagnostic << '\n';
        }
        return missingCount == 0 ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << "asset_fixture_check failed: " << error.what() << '\n';
        if (resultFile.has_value()) {
            try {
                vac::host::HostResult result;
                result.host = "asset_fixture_check";
                result.status = "error";
                result.message = error.what();
                result.diagnostics.push_back(std::string{"process_error: "} + error.what());
                vac::host::writeResultFile(*resultFile, result);
            } catch (const std::exception &resultError) {
                std::cerr << "asset_fixture_check could not write result file: " << resultError.what() << '\n';
            }
        }
        return 1;
    }
}
