#ifndef PIPELINE_CONFIG_HPP
#define PIPELINE_CONFIG_HPP

#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/exception/all.hpp>
#include <iostream>
#include <GstPipeContainer.hpp>
#include <InferNode.hpp>
#include <FrameControlNode.hpp>
#include <vector>

struct PipelineConfig{
std::vector<GstPipeContainer::Config> vDecConfig;
std::string guiSocket;
InferNode::Config detConfig;
InferNode::Config clsConfig;
FrameControlNode::Config FRCConfig;
};

class PipelineConfigParser{
public:
    PipelineConfigParser();

    bool parse(const std::string& filename);

    PipelineConfig get() const;

private:
    bool parseDecConfig();
    bool parseDetConfig();
    bool parseClsConfig();
    bool parseFRCConfig();
    bool parseGUIConfig();

    template <typename T>
    bool parseFromPTree(const boost::property_tree::ptree& ptree, const std::string& path, T& result);

    boost::property_tree::ptree m_ptree;
    std::string m_fileName;
    PipelineConfig m_config;
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