#include "volume_loader.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

Volume loadRAW(const std::string& path, int w, int h, int d)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
        throw std::runtime_error("Failed to open RAW file");

    size_t size = (size_t)w * h * d;

    std::vector<float> data(size);

    file.read(reinterpret_cast<char*>(data.data()), size * sizeof(float));

    if (!file)
        throw std::runtime_error("Failed to read RAW data");

    Volume vol;
    vol.width = w;
    vol.height = h;
    vol.depth = d;
    vol.data = std::move(data);

    std::cout << "Loaded RAW: "
              << w << "x" << h << "x" << d << std::endl;

    return vol;
}