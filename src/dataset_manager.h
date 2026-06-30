#pragma once
#include <string>
#include <vector>

class DatasetManager {
public:
    struct Dataset {
        std::string name;
        std::string rawPath;
        std::string metaPath;
        bool isReady; // true if .raw and .txt exist
    };

    DatasetManager();
    ~DatasetManager() = default;

    // Scan data/ for subdirectories containing .dcm files
    void scanDatasets();

    // Get list of available datasets
    const std::vector<Dataset>& getDatasets() const { return m_datasets; }

    // Check if a dataset is ready (raw+meta exist)
    bool isDatasetReady(const std::string& name) const;

    // Convert a dataset (calls Python script) – returns true on success
    bool convertDataset(const std::string& name);

private:
    std::vector<Dataset> m_datasets;
    std::string m_dataDir = "data";
    std::string m_pythonScript = "utils/convert_to_raw.py";
    std::string m_pythonExe = ".venv/bin/python"; // Adjust if needed (Windows: .venv/Scripts/python)
};