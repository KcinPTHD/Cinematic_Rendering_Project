#include "volume_loader.h"

#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <filesystem>

// ======================================================
// HELPERS
// ======================================================
static bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// chama python se necessário
static std::string ensureRAW(const std::string& inputPath) {
    if (endsWith(inputPath, ".raw")) {
        return inputPath;
    }

    std::filesystem::path inputAbs = std::filesystem::absolute(inputPath);
    std::filesystem::path outputAbs = inputAbs;
    outputAbs += ".raw";

    if (!std::filesystem::exists(outputAbs)) {

        std::filesystem::path exePath = std::filesystem::current_path();

        std::filesystem::path projectRoot = exePath;
        if (projectRoot.filename() == "bin") {
            projectRoot = projectRoot.parent_path();
        }

        std::filesystem::path scriptPath =
            projectRoot / "utils" / "convert_to_raw.py";

        std::filesystem::path pythonPath =
            projectRoot / "venv" / "Scripts" / "python.exe";

        std::string command =
            "cmd /C \"\"" + pythonPath.string() +
            "\" \"" + scriptPath.string() +
            "\" \"" + inputAbs.string() +
            "\" \"" + outputAbs.string() +
            "\"\"";

        std::cout << "Running: " << command << std::endl;

        int result = system(command.c_str());

        if (result != 0) {
            throw std::runtime_error("Python conversion failed");
        }
    }

    return outputAbs.string();
}

// ======================================================
// RAW LOADER
// ======================================================
Volume loadRAW(const std::string& path, int w, int h, int d) {
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    size_t size = static_cast<size_t>(w) * h * d;

    std::vector<unsigned char> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    if (!file) {
        throw std::runtime_error("Failed to read full volume data");
    }

    Volume vol;
    vol.width = w;
    vol.height = h;
    vol.depth = d;
    vol.data.resize(size);

    // normalização
    for (size_t i = 0; i < size; i++) {
        vol.data[i] = buffer[i] / 255.0f;
    }

    // DEBUG: verificar range real dos dados
    float minV = 1.0f;
    float maxV = 0.0f;

    for (float v : vol.data) {
        if (v < minV) minV = v;
        if (v > maxV) maxV = v;
    }

    std::cout << "Volume range: " << minV << " -> " << maxV << std::endl;

    return vol;
}

// ======================================================
// INTERNAL HELPERS
// ======================================================
static void computeBounds(const Volume& vol,
                          float& minV,
                          float& maxV)
{
    minV = 1e9f;
    maxV = -1e9f;

    for (float v : vol.data) {
        minV = std::min(minV, v);
        maxV = std::max(maxV, v);
    }
}

static void normalizeVolume(Volume& vol)
{
    float minV, maxV;
    computeBounds(vol, minV, maxV);

    float range = maxV - minV;
    if (range == 0.0f) range = 1.0f;

    for (auto& v : vol.data) {
        v = (v - minV) / range;
    }
}

// ======================================================
// PIPELINE
// ======================================================
Volume loadVolumeAuto(const std::string& path)
{
    std::string rawPath = ensureRAW(path);

    int w = 256;
    int h = 256;
    int d = 196;

    Volume vol = loadRAW(rawPath, w, h, d);

    normalizeVolume(vol);

    return vol;
}