#include <PipelineConfig.hpp>

PipelineConfigParser::PipelineConfigParser():m_ready(false){

}

bool PipelineConfigParser::parse(const std::string& filename, bool ImgConfig){
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
    
    if(!parseImgGUIConfig()  || !parseImgDetConfig() || !parseImgFRCConfig() || !parseImgPathConfig()){
        return false;
    }

    m_ready = true;
    return true;
}

bool PipelineConfigParser::parseImgDetConfig(){
    if(!parseFromPTree(m_ptree, "Detection.Model", m_config_img.detConfig.model)){
        std::cout<<"Error: No detection model specified in config.json. Use default network path"<<std::endl;
        return false;
    }
    m_config_img.detConfig.iePluginName = "kmb";
    m_config_img.detConfig.batchSize = 1;
    m_config_img.detConfig.inferReqNumber = 1;
    m_config_img.detConfig.threshold = 0.6;

    std::string str;
    if(parseFromPTree(m_ptree, "Detection.InferReqNumber", str)){
    	m_config_img.detConfig.inferReqNumber = std::stoi(str);
    }
    if (m_config_img.detConfig.inferReqNumber > 8)
    {
        m_config_img.detConfig.inferReqNumber = 8;
        std::cout<<"Warning: max infer request number is 8"<<std::endl;
    }
    if (m_config_img.detConfig.inferReqNumber < 1)
    {
        m_config_img.detConfig.inferReqNumber = 1;
        std::cout<<"Warning: min infer request numer is 1"<<std::endl;
    }

    if(parseFromPTree(m_ptree, "Detection.Threshold", str)){
    	m_config_img.detConfig.threshold = std::stof(str);
    }
    if (m_config_img.detConfig.threshold > 1.0f)
    {
        m_config_img.detConfig.threshold = 1.0f;
        std::cout<<"Warning: max threshold is 1"<<std::endl;
    }
    if (m_config_img.detConfig.threshold < 0.0f)
    {
        m_config_img.detConfig.threshold = 0.0f;
        std::cout<<"Warning: min threshold is 0"<<std::endl;
    }

    return true;
}

bool PipelineConfigParser::parseImgFRCConfig(){
    if(!parseFromPTree(m_ptree, "FRC.DropXFrame", m_config_img.FRCConfig.dropXFrame)){
        std::cout<<"Warning: No frame dropping count for FRC Node specified in config.json"<<std::endl;
        m_config_img.FRCConfig.dropXFrame = 0;
    }
    if(!parseFromPTree(m_ptree, "FRC.DropEveryXFrame", m_config_img.FRCConfig.dropEveryXFrame)){
    	m_config_img.FRCConfig.dropEveryXFrame = 1024;
    }

    return true;
}

bool PipelineConfigParser::parseImgGUIConfig(){
    if(!parseFromPTree(m_ptree, "GUI.Socket", m_config_img.guiSocket)){
        std::cout<<"No GUI Socket specified. Exit"<<std::endl;
        return false;
    }
    return true;
}

bool PipelineConfigParser::parseImgPathConfig() {
	auto imgStreams = m_ptree.get_child("Img");
	for (const auto& stream : imgStreams) {
		ImgworkloadConfig temp;

		if (!parseFromPTree(stream.second, "Path",
				temp.imageFolderPath)) {
			std::cout << "No Image Path specified. Exit" << std::endl;
			return false;
		}
		if (!parseFromPTree(stream.second, "iterNum",
				temp.iterNum)) {
			temp.iterNum = 1;
		}
		if (!parseFromPTree(stream.second, "width", temp.width)) {
			std::cout << "Please set Image width. Exit" << std::endl;
			return false;
		}
		if (!parseFromPTree(stream.second, "height",
				temp.height)) {
			std::cout << "Please set Image height. Exit" << std::endl;
			return false;
		}
		m_config_img.vImgWLConfig.push_back(std::move(temp));
	}
	return true;
}

ImgPipelineConfig PipelineConfigParser::Imgget() const{
	return m_config_img;
}
