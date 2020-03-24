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

    parseHvaCmd();

    return true;
}

bool ConfigParser::readConfigFile(const std::string& filePath)
{
    std::ifstream input(filePath);
    if (!input.is_open()) {
        printf("not open\n");
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
    } catch (const boost::property_tree::ptree_error& e) {
        printf("ptree error\n");
        return false;
    } catch (boost::exception& e) {
        printf("error\n");
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

std::string ConfigParser::getHvaCMd()
{
    return m_hvaCmd;
}

template <typename Type>
bool ConfigParser::parse(const std::string& path, Type& result)
{
    try {
        result = m_ptree.get<Type>(path);
        return true;
    } catch (...) {
        printf("parse error");
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
        printf("parse error");
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
        printf("parse error");
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

bool ConfigParser::parseHvaCmd()
{
    parse("app.hva_cmd", m_hvaCmd);
    return true;
}

static ConfigParser::PlayMode strToPlayMode(const std::string& str)
{
    if (str == "replay") {
        return ConfigParser::PlayMode::REPLAY;
    } else {
        return ConfigParser::PlayMode::DEFAULT;
    }
}
