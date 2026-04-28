#pragma once
#include <vector>
#include <string>

struct Volume {
    int width = 0;
    int height = 0;
    int depth = 0;

    std::vector<float> data;
};

Volume loadRAW(const std::string& path, int w, int h, int d);