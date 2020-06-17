/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef HDDLDEMO_CONFIGPARSER_H
#define HDDLDEMO_CONFIGPARSER_H

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <map>
#include <string>
#include <vector>

class ConfigParser {
public:
    enum class PlayMode {
        DEFAULT,
        REPLAY
    };

    static ConfigParser* instance()
    {
        static ConfigParser parser;
        return &parser;
    }

    bool loadConfigFile(const std::string& filePath);

    int getTimeout();
    PlayMode getPlayMode();
    std::vector<std::string> getPipelines();
    std::vector<std::map<std::string, std::string>> getPipelineParams();

    bool isHvaConfigured();
    std::string getHvaCMd();
    std::string getHvaWorkDirectory();
    std::string getHvaSocketPath();
    std::map<std::string, std::string> getHvaEnvironmentVariables();

private:
    ConfigParser() = default;

    bool readConfigFile(const std::string& filePath);
    template <typename Type>
    bool parse(const std::string& path, Type& result);
    template <typename Type>
    bool parseList(const std::string& path, const std::string& subPath, std::vector<Type>& result);

    bool parsePipelines();
    bool parseTimeout();
    bool parsePlayMode();
    void insertPipelineParams();

    bool parseHvaCmd();
    bool parseHvaWorkDirectory();
    bool parseHvaSocketPath();
    bool parseHvaEnvironmentVariables();
    void parseHaveConfig();

    int m_timeout { 0 };
    PlayMode m_playMode { PlayMode::REPLAY };
    std::vector<std::string> m_pipelines {};
    std::vector<std::map<std::string, std::string>> m_params {};

    std::string m_hvaCmd {};
    std::string m_hvaWorkDirectory {};
    std::string m_hvaSocketPath {};
    std::map<std::string, std::string> m_hvaEnvironmentVariables {};
    bool m_hvaEnabled { false };

    boost::property_tree::ptree m_ptree;
};

#endif //HDDLDEMO_CONFIGPARSER_H
