//
// Created by xiao on 2020/2/11.
//
#include <boost/format.hpp>
#include <fstream>

#include "ConfigParser.h"

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

    if (!parseClassficationModelPath()) {
        return false;
    }

    if (!parseDetectionModelPath()) {
        return false;
    }

    if (!parseMediaFiles()) {
        return false;
    }
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

int ConfigParser::getTimeout()
{
    return m_timeout;
}

ConfigParser::PlayMode ConfigParser::getPlayMode()
{
    return m_playMode;
}

std::string ConfigParser::getDetectionModelPath()
{
    return m_detectionModelPath;
}

std::string ConfigParser::getClassificationModelPath()
{
    return m_classificationModelPath;
}

std::vector<std::string> ConfigParser::getMediaFiles()
{
    return m_mediaFiles;
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

bool ConfigParser::parsePipelines()
{
    return parseList("decode", "pipe", m_pipelines);
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

bool ConfigParser::parseDetectionModelPath()
{
    return parse("app.detection", m_detectionModelPath);
}

bool ConfigParser::parseClassficationModelPath()
{
    return parse("app.classification", m_classificationModelPath);
}

bool ConfigParser::parseMediaFiles()
{
    return parseList("app.src", "path", m_mediaFiles);
}

static ConfigParser::PlayMode strToPlayMode(const std::string& str)
{
    if (str == "replay") {
        return ConfigParser::PlayMode::REPLAY;
    } else {
        return ConfigParser::PlayMode::DEFAULT;
    }
}
