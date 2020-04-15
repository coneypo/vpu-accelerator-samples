//
// Created by xiao on 2020/2/11.
//
#include <boost/format.hpp>
#include <iostream>

#include "configparser.h"

static ConfigParser::PlayMode strToPlayMode(const std::string& str);

bool ConfigParser::loadConfigFile(const std::string& filePath)
{
    if (!readConfigFile(filePath)) {
        return false;
    }

    if (!parsePipelines()) {
        return false;
    }

    if (!parsePlayMode()) {
        return false;
    }

    if (!parseTimeout()) {
        return false;
    }

    parseHaveConfig();
    return true;
}

bool ConfigParser::readConfigFile(const std::string& filePath)
{
    std::ifstream input(filePath);
    if (!input.is_open()) {
        std::cerr << "cannot open file" << filePath << std::endl;
        return false;
    }

    std::string line;
    std::stringstream output;
    while (std::getline(input, line)) {
        auto pos = line.find("//");
        if (pos != std::string::npos) {
            line.erase(pos);
        }
        output << line << '\n';
    }

    try {
        boost::property_tree::read_json(output, m_ptree);
    } catch (...) {
        std::cerr << "Json format error!" << std::endl;
        return false;
    }
    return true;
}

std::vector<std::string> ConfigParser::getPipelines()
{
    return m_pipelines;
}

std::vector<std::map<std::string, std::string>> ConfigParser::getPipelineParams()
{
    return m_params;
}

int ConfigParser::getTimeout()
{
    return m_timeout;
}

ConfigParser::PlayMode ConfigParser::getPlayMode()
{
    return m_playMode;
}

template <typename Type>
bool ConfigParser::parse(const std::string& path, Type& result)
{
    try {
        result = m_ptree.get<Type>(path);
        return true;
    } catch (...) {
        std::cerr << "Missing config item:" << path << std::endl;
        return false;
    }
}

template <typename Type>
bool ConfigParser::parseList(const std::string& path, const std::string& subPath, std::vector<Type>& result)
{
    try {
        auto listNode = m_ptree.get_child(path);
        for (auto& entry : listNode) {
            result.push_back(entry.second.get<Type>(subPath));
        }
        return true;
    } catch (...) {
        std::cerr << "Parse config item failed: " << path << "." << subPath << std::endl;
        return false;
    }
}

template <>
bool ConfigParser::parseList(const std::string& path, const std::string& subPath, std::vector<std::map<std::string, std::string>>& result)
{
    try {
        auto listNode = m_ptree.get_child(path);
        for (auto& entry : listNode) {
            auto& subNode = entry.second.get_child(subPath);
            std::map<std::string, std::string> param;
            for (auto& item : subNode) {
                param.insert(std::make_pair(item.first, item.second.data()));
            }
            result.push_back(std::move(param));
        }
        return true;
    } catch (...) {
        std::cerr << "Parse config item failed: " << path << "." << subPath << std::endl;
        return false;
    }
}

bool ConfigParser::parsePipelines()
{
    if (!parseList("decode", "pipe", m_pipelines)) {
        return false;
    }

    if (!parseList("decode", "param", m_params)) {
        return false;
    }
    insertPipelineParams();
    return true;
}

void ConfigParser::insertPipelineParams()
{
    for (size_t index = 0; index < m_pipelines.size(); index++) {
        for (auto& item : m_params[index]) {
            std::string param = "${" + item.first + "}";
            auto found = m_pipelines[index].find(param);
            while (found != std::string::npos) {
                m_pipelines[index].replace(found, param.length(), item.second);
                found = m_pipelines[index].find(param, found + 1);
            }
        }
    }
}

bool ConfigParser::parseTimeout()
{
    return parse("app.timeout", m_timeout);
}

bool ConfigParser::parsePlayMode()
{
    std::string modeStr;
    if (!parse("app.mode", modeStr)) {
        return false;
    }
    m_playMode = strToPlayMode(modeStr);
    return true;
}

std::string ConfigParser::getHvaCMd()
{
    return m_hvaCmd;
}

std::string ConfigParser::getHvaWorkDirectory()
{
    return m_hvaWorkDirectory;
}

std::string ConfigParser::getHvaSocketPath()
{
    return m_hvaSocketPath;
}

std::map<std::string, std::string> ConfigParser::getHvaEnvironmentVariables()
{
    return m_hvaEnvironmentVariables;
}

bool ConfigParser::isHvaConfigured()
{
    return m_hvaEnabled;
}

bool ConfigParser::parseHvaCmd()
{
    return parse("hva.cmd", m_hvaCmd);
}

bool ConfigParser::parseHvaWorkDirectory()
{
    return parse("hva.work_directory", m_hvaWorkDirectory);
}

bool ConfigParser::parseHvaSocketPath()
{
    return parse("hva.socket_name", m_hvaSocketPath);
}

bool ConfigParser::parseHvaEnvironmentVariables()
{
    try {
        auto listNode = m_ptree.get_child("hva.environment");
        for (auto& entry : listNode) {
            auto environmentVariableName = entry.second.get<std::string>("key");
            auto environmentVariableValue = entry.second.get<std::string>("value");
            m_hvaEnvironmentVariables.insert(std::make_pair(environmentVariableName, environmentVariableValue));
        }
        return true;
    } catch (...) {
        std::cerr << "Missing config item: hva.environment" << std::endl;
        return false;
    }
}

void ConfigParser::parseHaveConfig()
{
    bool ret = parseHvaCmd();
    ret = ret & parseHvaWorkDirectory();
    ret = ret & parseHvaSocketPath();
    ret = ret & parseHvaEnvironmentVariables();
    if (ret) {
        m_hvaEnabled = true;
        std::cout << "Get hva related configurations, run in by-pass mode" << std::endl;
    } else {
        m_hvaEnabled = false;
        std::cout << "hva related configurations not found, run in streaming mode" << std::endl;
    }
}

static ConfigParser::PlayMode strToPlayMode(const std::string& str)
{
    if (str == "replay") {
        return ConfigParser::PlayMode::REPLAY;
    } else {
        return ConfigParser::PlayMode::DEFAULT;
    }
}
