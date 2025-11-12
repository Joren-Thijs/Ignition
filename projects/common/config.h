#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <json.hpp>

struct IgnitionConfig {
    std::string server_exe;
    std::string driver_dll;
    std::vector<std::string> wine_cmd;
};

inline bool ParseConfig(const std::string& config_path, IgnitionConfig& config) {
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        std::cerr << "Failed to open ignition.json" << std::endl;
        return false;
    }

    nlohmann::json config_json;
    try {
        config_file >> config_json;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse ignition.json: " << e.what() << std::endl;
        return false;
    }

    if (config_json.contains("server_exe")) {
        config.server_exe = config_json["server_exe"];
    } else {
        std::cerr << "ignition.json is missing 'server_exe'" << std::endl;
        return false;
    }

    if (config_json.contains("driver_dll")) {
        config.driver_dll = config_json["driver_dll"];
    } else {
        std::cerr << "ignition.json is missing 'driver_dll'" << std::endl;
        return false;
    }

    if (config_json.contains("wine_cmd")) {
        config.wine_cmd = config_json["wine_cmd"].get<std::vector<std::string>>();
    }
    return true;
}