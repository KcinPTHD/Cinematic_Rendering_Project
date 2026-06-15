#pragma once
#include <fstream>
#include <string>

class DebugLogger {
public:
    static void init(const std::string& path) {
        file.open(path);
    }

    static void log(const std::string& msg) {
        if (file.is_open())
            file << msg << std::endl;
    }

    static void close() {
        if (file.is_open())
            file.close();
    }

private:
    static std::ofstream file;
};