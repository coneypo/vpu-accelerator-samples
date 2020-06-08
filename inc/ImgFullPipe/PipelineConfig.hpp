#ifndef PIPELINE_CONFIG_HPP
#define PIPELINE_CONFIG_HPP

#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/exception/all.hpp>
#include <iostream>
#include <ImgInferNode.hpp>
#include <ImgFrameControlNode.hpp>
#include <vector>

struct ImgworkloadConfig {
	std::string imageFolderPath;
	int iterNum;
	int width;
	int height;
};

struct ImgPipelineConfig {
	std::string guiSocket;
	ImgInferNode::Config detConfig;
	ImgFrameControlNode::Config FRCConfig;
	std::vector<ImgworkloadConfig> vImgWLConfig;
};

// utility class for parsing a json config file
class PipelineConfigParser{
public:
    PipelineConfigParser();

    /**
    * @brief parse a json file. This should be called before any get() call
    * 
    * @param filename config file name to be parsed
    * @return status
    * 
    */
    bool parse(const std::string& filename, bool imgConfig = false);

    /**
    * @brief get the parsed configuration
    * 
    * @param void
    * @return parsed configuration
    * 
    */
    ImgPipelineConfig Imgget() const;

private:

    bool parseImgDetConfig();
    bool parseImgFRCConfig();
    bool parseImgGUIConfig();
    bool parseImgPathConfig();

    template <typename T>
    bool parseFromPTree(const boost::property_tree::ptree& ptree, const std::string& path, T& result);

    boost::property_tree::ptree m_ptree;
    std::string m_fileName;
    ImgPipelineConfig m_config_img;
    bool m_ready;
};

template <typename T>
bool PipelineConfigParser::parseFromPTree(const boost::property_tree::ptree& ptree, const std::string& path, T& result){

    try {
        result = ptree.get<T>(path);
    } catch (const boost::property_tree::ptree_bad_path& e) {
        std::cout<<"Warn: parse failed, "<<e.what()<<std::endl;
        return false;
    } catch (const boost::property_tree::ptree_bad_data& e) {
        std::cout<<"Warn: parse failed, "<<e.what()<<std::endl;
        return false;
    } catch (boost::exception& e) {
        std::cout<<"Error: unclassified error, "<<boost::diagnostic_information(e)<<std::endl;
        return false;
    }
    return true;
}

#endif //#ifndef PIPELINE_CONFIG_HPP
