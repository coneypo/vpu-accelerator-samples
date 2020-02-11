//
// Created by xiao on 2020/2/11.
//

#ifndef HDDLDEMO_CONFIGPARSER_H
#define HDDLDEMO_CONFIGPARSER_H

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
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
    std::vector<std::string> getMediaFiles();
    std::string getDetectionModelPath();
    std::string getClassificationModelPath();

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
    bool parseDetectionModelPath();
    bool parseClassficationModelPath();
    bool parseMediaFiles();

    std::vector<std::string> m_pipelines {};
    std::vector<std::string> m_mediaFiles {};
    int m_timeout { 0 };
    PlayMode m_playMode { PlayMode::REPLAY };
    std::string m_detectionModelPath {};
    std::string m_classificationModelPath {};

    boost::property_tree::ptree m_ptree;
};

#endif //HDDLDEMO_CONFIGPARSER_H
