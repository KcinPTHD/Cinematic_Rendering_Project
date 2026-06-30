#include "dataset_manager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <sstream>

namespace fs = std::filesystem;

DatasetManager::DatasetManager() {
    scanDatasets();
}

void DatasetManager::scanDatasets() {
    m_datasets.clear();
    if (!fs::exists(m_dataDir) || !fs::is_directory(m_dataDir))
        return;

    for (const auto& entry : fs::directory_iterator(m_dataDir)) {
        if (entry.is_directory()) {
            std::string name = entry.path().filename().string();
            // Check if directory contains .dcm files
            bool hasDicom = false;
            for (const auto& sub : fs::directory_iterator(entry.path())) {
                if (sub.path().extension() == ".dcm") {
                    hasDicom = true;
                    break;
                }
            }
            if (hasDicom) {
                Dataset ds;
                ds.name = name;
                ds.rawPath = m_dataDir + "/" + name + ".raw";
                ds.metaPath = m_dataDir + "/" + name + ".txt";
                ds.isReady = (fs::exists(ds.rawPath) && fs::exists(ds.metaPath));
                m_datasets.push_back(ds);
            }
        }
    }
}

bool DatasetManager::isDatasetReady(const std::string& name) const {
    for (const auto& ds : m_datasets) {
        if (ds.name == name)
            return ds.isReady;
    }
    return false;
}

bool DatasetManager::convertDataset(const std::string& name) {
    // Find the dataset entry
    Dataset* target = nullptr;
    for (auto& ds : m_datasets) {
        if (ds.name == name) {
            target = &ds;
            break;
        }
    }
    if (!target) return false;

    std::string inputDir = m_dataDir + "/" + name;
    std::string outputPrefix = m_dataDir + "/" + name;

    // Build command
    std::ostringstream cmd;
    cmd << m_pythonExe << " " << m_pythonScript
        << " --input-dir " << inputDir
        << " --output-prefix " << outputPrefix;

    std::cout << "[DatasetManager] Converting: " << cmd.str() << std::endl;

    int result = std::system(cmd.str().c_str());
    if (result == 0) {
        target->isReady = true;
        return true;
    } else {
        std::cerr << "[DatasetManager] Conversion failed for " << name << std::endl;
        return false;
    }
}