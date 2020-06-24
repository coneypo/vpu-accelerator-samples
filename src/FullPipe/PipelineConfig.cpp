#include <PipelineConfig.hpp>

PipelineConfigParser::PipelineConfigParser():m_ready(false){

}

bool PipelineConfigParser::parse(const std::string& filename){
    std::ifstream input(filename);
    if(!input.is_open()){
        std::cout<<"Fail to open "<<m_fileName<<std::endl;
        return false;
    }
    m_fileName = filename;

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
    
    if(!parseGUIConfig() || !parseDecConfig() || !parseDetConfig() || !parseClsConfig() || !parseFRCConfig()){
        return false;
    }

    m_ready = true;
    return true;
}

bool PipelineConfigParser::parseDecConfig(){
    auto decStreams = m_ptree.get_child("Decode");
    for(const auto& stream: decStreams){
        GstPipeContainer::Config temp;

        if(!parseFromPTree(stream.second, "Video", temp.filename)){
            std::cout<<"Error: No input video file specified in config.json"<<std::endl;
            return false;
        }

        if(!parseFromPTree(stream.second, "Codec", temp.codec)){
            std::cout<<"Error: No input video codec specified in config.json"<<std::endl;
            return false;
        }

        // if(!parseFromPTree(stream.second, "DropEveryXFrame", temp.dropEveryXFrame)){
        //     std::cout<<"Warning: No frame dropping count for decoder specified in config.json"<<std::endl;
        //     temp.dropEveryXFrame = 0;
        // }
        temp.dropEveryXFrame = 1024;

        // if(!parseFromPTree(stream.second, "DropXFrame", temp.dropXFrame)){
        //     temp.dropXFrame = 1024;
        // }
        temp.dropXFrame = 0;

        temp.enableFpsCounting = true;

        m_config.vDecConfig.push_back(std::move(temp));
    }
    return true;
}

bool PipelineConfigParser::parseDetConfig(){
    if(!parseFromPTree(m_ptree, "Detection.Model", m_config.detConfig.model)){
        std::cout<<"Error: No detection model specified in config.json. Use default network path"<<std::endl;
        return false;
    }
    m_config.detConfig.iePluginName = "kmb";
    m_config.detConfig.batchSize = 1;
    m_config.detConfig.inferReqNumber = 1;
    m_config.detConfig.threshold = 0.6;

    std::string str;
    if(parseFromPTree(m_ptree, "Detection.InferReqNumber", str)){
        m_config.detConfig.inferReqNumber = std::stoi(str);
    }
    if (m_config.detConfig.inferReqNumber > 8)
    {
        m_config.detConfig.inferReqNumber = 8;
        std::cout<<"Warning: max infer request number is 8"<<std::endl;
    }
    if (m_config.detConfig.inferReqNumber < 1)
    {
        m_config.detConfig.inferReqNumber = 1;
        std::cout<<"Warning: min infer request numer is 1"<<std::endl;
    }

    if(parseFromPTree(m_ptree, "Detection.Threshold", str)){
        m_config.detConfig.threshold = std::stof(str);
    }
    if (m_config.detConfig.threshold > 1.0f)
    {
        m_config.detConfig.threshold = 1.0f;
        std::cout<<"Warning: max threshold is 1"<<std::endl;
    }
    if (m_config.detConfig.threshold < 0.0f)
    {
        m_config.detConfig.threshold = 0.0f;
        std::cout<<"Warning: min threshold is 0"<<std::endl;
    }

    return true;
}

bool PipelineConfigParser::parseClsConfig(){
    if(!parseFromPTree(m_ptree, "Classification.Model", m_config.clsConfig.model)){
        std::cout<<"Error: No Classification model specified in config.json. Use default network path"<<std::endl;
        return false;
    }
    m_config.clsConfig.iePluginName = "kmb";
    m_config.clsConfig.batchSize = 1;
    m_config.clsConfig.inferReqNumber = 1;
    
    std::string str;
    if(parseFromPTree(m_ptree, "Classification.InferReqNumber", str)){
        m_config.clsConfig.inferReqNumber = std::stoi(str);
    }
    if (m_config.clsConfig.inferReqNumber > 8)
    {
        m_config.clsConfig.inferReqNumber = 8;
        std::cout<<"Warning: max infer request number is 8"<<std::endl;
    }
    if (m_config.clsConfig.inferReqNumber < 1)
    {
        m_config.clsConfig.inferReqNumber = 1;
        std::cout<<"Warning: min infer request numer is 1"<<std::endl;
    }
    
    return true;
}

bool PipelineConfigParser::parseFRCConfig(){
    if(!parseFromPTree(m_ptree, "FRC.DropXFrame", m_config.FRCConfig.dropXFrame)){
        std::cout<<"Warning: No frame dropping count for FRC Node specified in config.json"<<std::endl;
        m_config.FRCConfig.dropXFrame = 0;
    }
    if(!parseFromPTree(m_ptree, "FRC.DropEveryXFrame", m_config.FRCConfig.dropEveryXFrame)){
        m_config.FRCConfig.dropEveryXFrame = 1024;
    }

    return true;
}

bool PipelineConfigParser::parseGUIConfig(){
    if(!parseFromPTree(m_ptree, "GUI.Socket", m_config.guiSocket)){
        std::cout<<"No GUI Socket specified. Exit"<<std::endl;
        return false;
    }
    return true;
}

PipelineConfig PipelineConfigParser::get() const{
    return m_config;
}
