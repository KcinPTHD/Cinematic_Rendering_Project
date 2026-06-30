#pragma once
#include <vector>
#include <string>

struct Volume {
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int depth = 0;
    std::vector<float> data;
};

Volume loadRAW(const std::string& path, int w, int h, int d);