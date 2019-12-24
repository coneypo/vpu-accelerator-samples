//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_JSONPARSER_H
#define HDDLUNITE_JSONPARSER_H

#include <boost/exception/all.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <istream>
#include <memory>
#include <sstream>

#include "utils/FileUtils.h"
#include "utils/HLog.h"

namespace HddlUnite {
class JsonParser {
public:
    using Ptr = std::shared_ptr<JsonParser>;

    enum class Error : uint8_t {
        NONE = 0,
        NOT_LOADED = 1,
        FILE_NOT_EXIST = 2,
        OPEN_FILE_ERROR = 3,
        NOT_IN_JSON_FORMAT = 4,
        NO_SUCH_NODE = 5,
        TYPE_CONVERSION_FAILED = 6,
        UNCLASSIFIED_ERROR = 10
    };

    virtual ~JsonParser() = default;

    bool isLoaded() const;
    std::string getConfigFile() const;
    Error loadConfig(const std::string& filePath);

    template <typename Type>
    Error parse(const std::string& path, Type& result);

    static std::string strError(Error error);
    static Ptr create();

private:
    std::string m_configFile;
    boost::property_tree::ptree m_ptree;
};

inline bool JsonParser::isLoaded() const
{
    return !m_configFile.empty();
}

inline std::string JsonParser::getConfigFile() const
{
    return m_configFile;
}

inline JsonParser::Error JsonParser::loadConfig(const std::string& filePath)
{
    if (!FileUtils::exist(filePath)) {
        HError("Error: '%s' doesn't exist", filePath);
        return Error::FILE_NOT_EXIST;
    }

    std::ifstream input(filePath);

    if (!input.is_open()) {
        HError("Error: open '%s' failed", filePath);
        return Error::OPEN_FILE_ERROR;
    }

    /*
     * Boost's 1.59 property tree based JSON parser does not accept comments.
     * For semi-compatibility with existing configurations we will attempt to strip
     * C++ style comments.
     */

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
        HError("Error: load config failed, %s", e.what());
        return Error::NOT_IN_JSON_FORMAT;
    } catch (boost::exception& e) {
        HError("Error: unclassified error.\n%s", boost::diagnostic_information(e));
        return Error::UNCLASSIFIED_ERROR;
    }

    if (m_ptree.empty()) {
        HError("Error: config file '%s' is empty", filePath);
        return Error::NOT_IN_JSON_FORMAT;
    }

    m_configFile = filePath;

    return Error::NONE;
}

template <typename ParseType>
inline JsonParser::Error JsonParser::parse(const std::string& path, ParseType& result)
{
    if (m_configFile.empty()) {
        return Error::NOT_LOADED;
    }

    try {
        result = m_ptree.get<ParseType>(path);
    } catch (const boost::property_tree::ptree_bad_path& e) {
        HDebug("Warn: parse failed, %s", e.what());
        return Error::NO_SUCH_NODE;
    } catch (const boost::property_tree::ptree_bad_data& e) {
        HError("Error: parse failed, %s", e.what());
        return Error::TYPE_CONVERSION_FAILED;
    } catch (boost::exception& e) {
        HError("Error: unclassified error.\n%s", boost::diagnostic_information(e));
        return Error::UNCLASSIFIED_ERROR;
    }

    return Error::NONE;
}

inline std::string JsonParser::strError(Error error)
{
    switch (error) {
    case Error::NONE:
        return "NONE";
    case Error::NOT_LOADED:
        return "NOT_LOADED";
    case Error::FILE_NOT_EXIST:
        return "FILE_NOT_EXIST";
    case Error::OPEN_FILE_ERROR:
        return "OPEN_FILE_ERROR";
    case Error::NOT_IN_JSON_FORMAT:
        return "NOT_IN_JSON_FORMAT";
    case Error::NO_SUCH_NODE:
        return "NO_SUCH_NODE";
    case Error::TYPE_CONVERSION_FAILED:
        return "TYPE_CONVERSION_FAILED";
    case Error::UNCLASSIFIED_ERROR:
        return "UNCLASSIFIED_ERROR";
    default:
        return "UNKNOWN_ERROR";
    }
}

inline JsonParser::Ptr JsonParser::create()
{
    return std::make_shared<JsonParser>();
}
}

#endif //HDDLUNITE_JSONPARSER_H
