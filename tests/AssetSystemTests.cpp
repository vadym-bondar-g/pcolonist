#include "pcolonist/assets/AssetSystem.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

void runTerrainColliderTests();
void runObjLoaderTests();
void runWorldSystemTests();
void runInventoryTests();
void runPngLoaderTests();
void runPlayerVitalsTests();
void runResourceHarvestingTests();
void runBuildingPlacementTests();
void runSaveGameSerializerTests();
void runScriptSystemTests();

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
              / ("pcolonist-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

template <typename Function>
void requireThrows(Function&& function, std::string_view message) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error(std::string(message));
}

void testResolvesPathsInsideRoot() {
    TemporaryDirectory temporary;
    const std::filesystem::path root = temporary.path() / "assets";
    std::filesystem::create_directories(root / "scripts");
    std::ofstream(root / "scripts" / "startup.script") << "gravity 0 -9.81 0\n";

    const pcolonist::AssetSystem assets(root);
    require(
        assets.readText("scripts/../scripts/startup.script") == "gravity 0 -9.81 0\n",
        "AssetSystem must read normalized paths inside the asset root");
}

void testRejectsPathsOutsideRoot() {
    TemporaryDirectory temporary;
    const std::filesystem::path root = temporary.path() / "assets";
    std::filesystem::create_directories(root);
    std::ofstream(temporary.path() / "private.txt") << "private";

    const pcolonist::AssetSystem assets(root);
    requireThrows(
        [&assets] {
            static_cast<void>(assets.resolve("../private.txt"));
        },
        "AssetSystem must reject parent traversal");
    requireThrows(
        [&assets, &temporary] {
            static_cast<void>(assets.resolve(temporary.path() / "private.txt"));
        },
        "AssetSystem must reject absolute paths");
}

void testRejectsSymlinksOutsideRoot() {
    TemporaryDirectory temporary;
    const std::filesystem::path root = temporary.path() / "assets";
    const std::filesystem::path outside = temporary.path() / "outside";
    std::filesystem::create_directories(root);
    std::filesystem::create_directories(outside);
    std::ofstream(outside / "private.txt") << "private";

    std::error_code error;
    std::filesystem::create_directory_symlink(outside, root / "external", error);
    if (error) {
        return;
    }

    const pcolonist::AssetSystem assets(root);
    requireThrows(
        [&assets] {
            static_cast<void>(assets.resolve("external/private.txt"));
        },
        "AssetSystem must reject symlinks outside the asset root");
}

void testCachesBinaryAssetsInMemory() {
    TemporaryDirectory temporary;
    const std::filesystem::path root = temporary.path() / "assets";
    std::filesystem::create_directories(root / "data");
    std::ofstream(root / "data" / "value.bin", std::ios::binary) << "first";

    const pcolonist::AssetSystem assets(root);
    const std::vector<unsigned char>& first = assets.readBinary("data/value.bin");
    require(std::string(first.begin(), first.end()) == "first", "AssetSystem must read binary assets");

    std::ofstream(root / "data" / "value.bin", std::ios::binary | std::ios::trunc) << "second";
    const std::vector<unsigned char>& cached = assets.readBinary("data/value.bin");
    require(std::string(cached.begin(), cached.end()) == "first", "AssetSystem must serve cached binary assets from memory");
}

} // namespace

int main() {
    try {
        testResolvesPathsInsideRoot();
        testRejectsPathsOutsideRoot();
        testRejectsSymlinksOutsideRoot();
        testCachesBinaryAssetsInMemory();
        runTerrainColliderTests();
        runObjLoaderTests();
        runWorldSystemTests();
        runInventoryTests();
        runPngLoaderTests();
        runPlayerVitalsTests();
        runResourceHarvestingTests();
        runBuildingPlacementTests();
        runSaveGameSerializerTests();
        runScriptSystemTests();
    } catch (const std::exception& error) {
        std::cerr << "AssetSystem test failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
