#ifndef JSONPARSER_HPP
#define JSONPARSER_HPP

#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/exception/diagnostic_information.hpp>

class JsonParser {
public:
    JsonParser(std::string filename);

    // ~JsonParser();

    bool init();

    template <typename T>
    bool parse(const std::string& path, T& result);

private:
    std::string m_fileName;
    boost::property_tree::ptree m_ptree;
    bool m_bReady;
};

template <typename T>
bool JsonParser::parse(const std::string& path, T& result){
    if(!m_bReady){
        if(!init()){
            std::cout<<"Json Parser initiated failed!"<<std::endl;
            return false;
        }
    }

    try {
        result = m_ptree.get<T>(path);
    } catch (const boost::property_tree::ptree_bad_path& e) {
        std::cout<<"Warn: parse failed, "<<e.what()<<std::endl;
        return false;
    } catch (const boost::property_tree::ptree_bad_data& e) {
        std::cout<<"Warn: parse failed, "<<e.what()<<std::endl;
        return false;
    } catch (boost::exception& e) {
        std::cout<<"Error: unclassified error, "<<oost::diagnostic_information(e))<<std::endl;
        return false;
    }
    return true;
}

#endif //#ifndef JSONPARSER_HPP