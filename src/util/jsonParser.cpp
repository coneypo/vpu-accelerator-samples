#include <util/jsonParser.hpp>
#include <boost/exception/all.hpp>
#include <fstream>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>

JsonParser::JsonParser(std::string filename):m_fileName(filename) ,m_bReady(false){

}

bool JsonParser::init(){
    if(!m_bReady){
        std::ifstream input(m_fileName);
        if(!input.is_open()){
            std::cout<<"Fail to open "<<m_fileName<<std::endl;
            return false;
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
            std::cout<<"Error: load config failed, "<<e.what()<<std::endl;
            return false;
        } catch (boost::exception& e) {
            std::cout<<"Error: unclassified error, "<<boost::diagnostic_information(e)<<std::endl;
            return false;
        }

        if (m_ptree.empty()) {
            std::cout<<"Error: config file is empty"<<std::endl;
            return false;
        }
        m_bReady = true;
        return true;
    }
    else
        return true;
}