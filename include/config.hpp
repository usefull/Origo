#pragma once

#include <string>
#include <iostream>
#include <nlohmann/json.hpp>
#include <fmt/core.h>

#include "./messages.hpp"

using namespace std;
using json = nlohmann::json;
namespace err = origo::ErrorMessages;

namespace origo
{
    struct Config {
        string ip;
        uint port;
        unordered_map<string, std::filesystem::path> staticDirs;

        json to_json() const {
            return {{"ip", ip}, {"port", port}};
        }

        static Config from_json(const json& j) {

            Config config;

            try
            {
                config.ip = j.at("ip");
            }
            catch (const exception& e)
            {
                throw runtime_error(fmt::format(err::CantReadIpFromConfig, e.what()));
            }

            try
            {
                config.port = j.at("port");
            }
            catch (const exception& e)
            {
                throw runtime_error(fmt::format(err::CantReadPortFromConfig, e.what()));
            }

            try {
                // Узнаём пут к папке исполняемого файла.
                char buffer[PATH_MAX];
                auto len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
                buffer[len] = '\0';
                std::filesystem::path exePath(buffer);
                std::filesystem::path exeDir = exePath.parent_path();

                // В поле staticDirs пишем полные пути к папкам со статическими файлами из конфига.
                auto itStaticDirs = j.find("staticDirs");
                if (itStaticDirs != j.end()) {
                    for (auto& [key, value] : itStaticDirs.value().items()) {
                        if (value.is_string())
                            config.staticDirs.insert({ key, exeDir / value.get<std::string>() });
                        else
                            config.staticDirs.insert({ key, exeDir / key });
                    }
                }
            }
            catch (const exception& e) {
                throw runtime_error(fmt::format(err::CantReadStaticDirsFromConfig, e.what()));
            }

            return config;
        }

        static Config from_file(const char* file_path)
        {
            ifstream file(file_path);
            if (!file)
            {
                throw runtime_error(fmt::format(err::CantOpenConfigFile, file_path));
            }

            json jsonConfig;
            try
            {
                jsonConfig = json::parse(file, nullptr, true, true);
            }
            catch (const json::parse_error& e)
            {
                throw runtime_error(fmt::format(err::ConfigFileReadingError, e.what()));
            }

            return Config::from_json(jsonConfig);
        }
    };
}