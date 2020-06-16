#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <string>
#include <future>
#include <mutex>
#include <condition_variable>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <hvaPipeline.hpp>

#include <ipc.h>
#include <jsonParser.hpp>
#include <hddl2plugin_helper.hpp>
#include <PipelineConfig.hpp>
#include <validationDumpNode.hpp>

#include <ImgSenderNode.hpp>
#include <ImgFrameControlNode.hpp>
#include <ImgInferNode.hpp>
// for Image fold
#include <iostream>
#include <sys/stat.h>
#ifdef _WIN32
#include <os/windows/w_dirent.h>
#else
#include <dirent.h>
#endif

using ms = std::chrono::milliseconds;

/**
 * @Socket Control message
 */
enum ControlMessage{
    EMPTY = 0,
    ADDR_RECVED,
    STOP_RECVED
};

std::mutex g_mutex;
std::condition_variable g_cv;

/**
 * @brief Check graph file validity
 * @param filepath Graph file path
 * @return Validity
 */
bool checkValidNetFile(std::string& filepath){
    std::string::size_type suffix_pos_blob = filepath.find(".blob");
    std::string::size_type suffix_pos_xml = filepath.find(".xml");
    if(suffix_pos_blob == std::string::npos && suffix_pos_xml == std::string::npos){
        std::cout<<"Invalid network suffix. Expect *.xml or *.blob"<<std::endl;
        HVA_ERROR("Invalid network suffix. Expect *.xml or *.blob");
        return false;
    }

    if(suffix_pos_blob == std::string::npos){
        auto blobPath = filepath.substr(0, filepath.length() - 4) + ".blob";
        std::ifstream f(blobPath.c_str());
        if (!f.good())
        {
            std::string binFilepath = filepath.substr(0, filepath.length() - 4) + ".bin";

            auto p = boost::filesystem::path(filepath);
            if(!boost::filesystem::exists(p) || !boost::filesystem::is_regular_file(p)){
                std::cout<<"File "<<filepath<<" does not exists"<<std::endl;
                HVA_ERROR("File %s does not exists", filepath.c_str());
                return false;
            }

            auto p_bin = boost::filesystem::path(binFilepath);
            if(!boost::filesystem::exists(p_bin) || !boost::filesystem::is_regular_file(p_bin)){
                std::cout<<"File "<<binFilepath<<" does not exists"<<std::endl;
                HVA_ERROR("File %s does not exists", binFilepath.c_str());
                return false;
            }

            HDDL2pluginHelper_t::compile(filepath);
        }
        filepath = blobPath;
    }
    else {
        auto p = boost::filesystem::path(filepath);
        if(!boost::filesystem::exists(p) || !boost::filesystem::is_regular_file(p)){
            std::cout<<"File "<<filepath<<" does not exists"<<std::endl;
            HVA_ERROR("File %s does not exists", filepath.c_str());
            return false;
        }
    }

    return true;
}

/**
 * Socket configuration
 */
struct SocketsConfig_t{
    unsigned numOfStreams;
    std::vector<std::string> unixSocket;
};

/**
 * @brief Socket configuration receive routine
 * @param socket_address Socket address for listening
 * @param config Received socket configuration
 * @return Status
 */
int receiveRoutine(const char* socket_address, ControlMessage* ctrlMsg, SocketsConfig_t* config)
{
    auto poller = HddlUnite::Poller::create();
    auto connection = HddlUnite::Connection::create(poller);
    std::cout<<"Set socket to listening"<<std::endl;
    HVA_DEBUG("Set socket to listening");
    if (!connection->listen(socket_address)) {
        return -1;
    }

    bool running = true;

    while (running) {
        auto event = poller->waitEvent(10);
        switch (event.type) {
            case HddlUnite::Event::Type::CONNECTION_IN:
                connection->accept();
                HVA_DEBUG("Incoming connection accepted");
                break;
            case HddlUnite::Event::Type::MESSAGE_IN: {

                int length = 0;
                auto& data_connection = event.connection;
                std::string message;
                {
                    std::lock_guard<std::mutex> autoLock(data_connection->getMutex());

                    if (!data_connection->read(&length, sizeof(length))) {
                        break;
                    }
                    HVA_DEBUG("Length received is %d", length);
                    if (length <= 0) {
                        break;
                    }

                    message = std::string(static_cast<size_t>(length), ' ');
                    if (!data_connection->read(&message[0], length)) {
                        break;
                    }
                    HVA_DEBUG("message received is %s", message.c_str());
                }

                if(message=="STOP"){
                    /* to-do: stop pipeline*/
                }
                else{
                    /* start pipeline */
                    {
                        std::lock_guard<std::mutex> lg(g_mutex);
                        boost::split(config->unixSocket, message, boost::is_any_of(","));
                        for (unsigned i = 0; i < config->unixSocket.size();++i){
                            HVA_DEBUG("Value of config[%d]: %s", i, config->unixSocket[i].c_str());
                        }
                        config->numOfStreams = config->unixSocket.size();

                        *ctrlMsg = ControlMessage::ADDR_RECVED;
                        HVA_INFO("Control message set to addr_recved");
                    }
                    g_cv.notify_all();
                    HVA_INFO("Going to stop receive routine after 5 s");
                    std::this_thread::sleep_for(ms(5000));
                    running = false;
                }
                break;
            }
            case HddlUnite::Event::Type::CONNECTION_OUT:
                HVA_INFO("Going to stop receive routine after 5 s");
                std::this_thread::sleep_for(ms(5000));
                running = false;
                break;

            default:
                break;
        }
    }
    return 0;
}

/**
* @brief This function checks input args and existence of specified files in a given folder
* @param arg path to a file to be checked for existence
* @return files updated vector of verified input files
*/
void readInputFilesArguments(std::vector<std::string> &files,
		const std::string& arg) {
	struct stat sb;
	std::stringstream err;
	if (stat(arg.c_str(), &sb) != 0) {
		err << "File " << arg << " cannot be opened!" << std::endl;
		return;
	}
	if (S_ISDIR(sb.st_mode)) {
		DIR *dp;
		dp = opendir(arg.c_str());
		if (dp == nullptr) {
			err << "Directory " << arg << " cannot be opened!" << std::endl;
			return;
		}

		struct dirent *ep;
		while (nullptr != (ep = readdir(dp))) {
			std::string fileName = ep->d_name;
			if (fileName == "." || fileName == "..")
				continue;
			files.push_back(arg + "/" + ep->d_name);
		}
		closedir(dp);
	} else {
		files.push_back(arg);
	}
//	std::cout << "Files were added: " << files.size() << std::endl;
//	for (std::string filePath : files) {
//		std::cout << "    " << filePath << std::endl;
//	}
}

/**
* @brief This function generate Image buffer from file
* @param image_path Image path
* @param size Image buffer size
* @return Image buffer pointer
*/
char* readImageDataFromFile(const std::string& image_path, const int size) {
    std::ifstream file(image_path, std::ios_base::ate | std::ios_base::binary);
    if (!file.good() || !file.is_open()) {
        std::stringstream err;
        err << "Cannot access input image file. File path: " << image_path;
        throw std::runtime_error(err.str());
    }

    const size_t file_size = file.tellg();
    if (file_size < size) {
        std::stringstream err;
        err << "Invalid read size provided. File size: " << file_size << ", to read: " << size;
        throw std::runtime_error(err.str());
    }
    file.seekg(0);

    char* data(new char[size]);
    file.read(reinterpret_cast<char*>(data), size);

    return data;
}

/**
* @brief This function load Image buffer into HVA blob
* @param pipeline HVA pipeline
* @param file image files' address
* @param width Image width
* @param height Image height
* @param streamId Image streams(One stream for one image folder)
* @param Iteration numbers for images loop in the image folder
* @param ctrlMsg Control message
* @return Status
*/
int fillHvaBlobs(hva::hvaPipeline_t* pipeline, std::vector<std::string>* file,
		const int& width, const int& height, const int& streamId,
		const int& iterNum, unsigned int& streamsNum, ControlMessage* ctrlMsg) {
	unsigned long currentSize = 0;
	float decFps = 0.0;
	const int size = width * (height * 3 / 2);

#ifdef VALIDATION_DUMP
	ms beforeRead = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now().time_since_epoch());
#endif

	int m_frameIdx = 0;
	for (int i = 0; i < iterNum; i++) {
		for (auto it = file->begin(); it != file->end(); it++) {
			//get imagefile name
			std::string imgName;
			std::shared_ptr<hva::hvaBlob_t> blob(new hva::hvaBlob_t());

			if ((*it).find_last_of(".yuv") != std::string::npos) {
				size_t iPosEnd = (*it).find_last_of(".yuv") - 3;
				size_t iPosStart = (*it).find_last_of("/") + 1;
				imgName = (*it).substr(iPosStart, iPosEnd - iPosStart);
				std::cout << "ImageName: " << imgName << std::endl;
			} else {
				std::cout
						<< "Failed to load NV12 Image, please check Image Format!"
						<< std::endl;
				return -1;
			}

			std::chrono::time_point<std::chrono::steady_clock>  startPosProc = std::chrono::steady_clock::now();

			auto imgBuf = readImageDataFromFile(*it, size);
			char* imgBufAddr = reinterpret_cast<char*>(imgBuf);

			unsigned int workLoadCount = iterNum * file->size();

			blob->emplace<char, ImageMeta>(imgBufAddr, size, new ImageMeta {
				static_cast<unsigned int>(width), static_cast<unsigned int>(height), startPosProc
#ifdef VALIDATION_DUMP
					, beforeRead, ms(0)
#endif
					, false, imgName, workLoadCount, streamsNum },
					[](char* BlobimgBufAddr, ImageMeta* meta) {
						HVA_DEBUG("Preparing to destruct BufAddr %ul", BlobimgBufAddr);
						delete []BlobimgBufAddr;
						delete meta;
					});

			blob->streamId = streamId;
			blob->frameId = m_frameIdx;

			++m_frameIdx;

			pipeline->sendToPort(blob, "FRCNode", 0, ms(0));

			std::this_thread::sleep_for(ms(5)); //temp wait
		}
	}
	return 0;
}

int main(){

    hvaLogger.setLogLevel(hva::hvaLogger_t::LogLevel::ERROR);

    PipelineConfigParser configParser;
    if(!configParser.parse("config.json", true)){
        std::cout<<"Failed to parse config.json"<<std::endl;
        HVA_ERROR("Failed to parse config.json");
        return 0;
    }

    ImgPipelineConfig config = configParser.Imgget();

    if(!checkValidNetFile(config.detConfig.model)){
        return -1;
    }

    for(const auto& imgConfigItem: config.vImgWLConfig){
        if((imgConfigItem.width != 416) || (imgConfigItem.height != 416))
        {
        	std::cout << "input size only support 416*416 by now!" << std::endl;
            return -1;
        }
    }

    SocketsConfig_t sockConfig;
    sockConfig.numOfStreams = 0;
    ControlMessage ctrlMsg = ControlMessage::EMPTY;

    std::thread t(receiveRoutine, config.guiSocket.c_str(), &ctrlMsg, &sockConfig);

    hva::hvaPipeline_t pl;

    {
        std::unique_lock<std::mutex> lk(g_mutex);
        if(sockConfig.numOfStreams == 0){
            g_cv.wait(lk,[&](){ return ctrlMsg == ControlMessage::ADDR_RECVED;});
            std::cout<<"Control message addr_recved received and cleared"<<std::endl;
            HVA_INFO("Control message addr_recved received and cleared");
            ctrlMsg = ControlMessage::EMPTY;
        }
    }

    std::vector<uint64_t> vWID(sockConfig.numOfStreams, 0);
    vWID[0] = 0;

    t.join();

    if (config.vImgWLConfig.size() < sockConfig.numOfStreams)
    {
    	std::cout << "Not enough Imgconfig info to support all Streams!!" << std::endl;
        return -1;
    }

    std::cout<<"Start pipeline with "<<sockConfig.numOfStreams<<" streams."<<std::endl;
    HVA_INFO("Start pipeline with %d streams", sockConfig.numOfStreams);
    auto &FRCNode = pl.setSource(std::make_shared<ImgFrameControlNode>(1, 1, 1/*, config.FRCConfig*/), "FRCNode");

    auto &detNode = pl.addNode(std::make_shared<ImgInferNode>(1, 1, sockConfig.numOfStreams, vWID, config.detConfig.model, "detection",
                                                             &HDDL2pluginHelper_t::postprocYolotinyv2_fp16, config.detConfig.inferReqNumber, config.detConfig.threshold), "detNode");

    if(sockConfig.numOfStreams > 1){
        hva::hvaBatchingConfig_t batchingConfig;
        batchingConfig.batchingPolicy = hva::hvaBatchingConfig_t::BatchingWithStream;
        batchingConfig.batchSize = 1;
        batchingConfig.streamNum = sockConfig.numOfStreams;
        batchingConfig.threadNumPerBatch = 1;

        detNode.configBatch(batchingConfig);
    }

    if(sockConfig.numOfStreams > 1){
        pl.addNode(std::make_shared<ImgSenderNode>(1,1,sockConfig.numOfStreams,sockConfig.unixSocket), "sendNode");
    }
    else{
        pl.addNode(std::make_shared<ImgSenderNode>(1,1,1,sockConfig.unixSocket), "sendNode");
    }

#ifdef VALIDATION_DUMP
//    auto& validationDumpNode = pl.addNode(std::make_shared<ValidationDumpNode>(1,0,1,"Yolotinyv2"), "validationDumpNode");
#endif

    pl.linkNode("FRCNode", 0, "detNode", 0);
    pl.linkNode("detNode", 0, "sendNode", 0);

#ifdef VALIDATION_DUMP
//    pl.linkNode("detNode", 1, "validationDumpNode", 0);
#endif

    pl.prepare();
    pl.start();

    std::cout<<"\nPipeline Start: "<<std::endl;

    std::vector<std::thread*> vTh;
    vTh.reserve(sockConfig.numOfStreams);

    for(unsigned i = 0; i < sockConfig.numOfStreams; ++i){
        vTh.push_back(new std::thread([&, i](){
            std::string imageFolder = config.vImgWLConfig[i].imageFolderPath;
            const int inputWidth = config.vImgWLConfig[i].width;
            const int inputHeight = config.vImgWLConfig[i].height;
            const int iterNum = config.vImgWLConfig[i].iterNum;

            std::vector<std::string> inputFiles;
            readInputFilesArguments(inputFiles, imageFolder);
            fillHvaBlobs(&pl, &inputFiles, inputWidth, inputHeight, i, iterNum, sockConfig.numOfStreams, &ctrlMsg);
        }));
    }

    for(unsigned i =0; i < sockConfig.numOfStreams; ++i){
        vTh[i]->join();
    }

    std::cout<<"Going to stop pipeline."<<std::endl;
    HVA_INFO("Going to stop pipeline");

    std::this_thread::sleep_for(ms(2000)); // to stop the pipeline

    pl.stop();

    std::cout<<"Pipeline has been stopped successfully."<<std::endl;

    return 0;
}
