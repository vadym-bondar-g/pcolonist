#include "pcolonist/assets/AssetSystem.hpp"
#include "pcolonist/assets/ObjLoader.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

void runObjLoaderTests() {
    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("pcolonist-obj-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    try {
        std::ofstream(root / "material.mtl")
            << "newmtl surface\n"
            << "Kd 0.4 0.5 0.6\n"
            << "Ks 0.3 0.6 0.9\n"
            << "Ns 200\n"
            << "Ke 0.1 0.2 0.3\n";
        std::ofstream(root / "mesh.obj")
            << "mtllib material.mtl\n"
            << "v 0 0 0\n"
            << "v 1 0 0\n"
            << "v 0 1 0\n"
            << "usemtl surface\n"
            << "f 1 2 3\n";

        const pcolonist::Mesh mesh = pcolonist::ObjLoader::load(pcolonist::AssetSystem(root), "mesh.obj");
        if (mesh.draws.size() != 1
            || std::abs(mesh.draws[0].specularStrength - 0.6F) > 0.001F
            || mesh.draws[0].roughness >= 0.2F
            || std::abs(mesh.draws[0].emissive.g - 0.2F) > 0.001F) {
            throw std::runtime_error("OBJ loader must preserve MTL lighting properties");
        }
    } catch (...) {
        std::filesystem::remove_all(root);
        throw;
    }
    std::filesystem::remove_all(root);
}
