#include "PipelineManager.h"
#include "XLinkConnector.h"
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <string>

using namespace hddl;

static int usage()
{
    std::cerr << "hddl_manager [-id|--id socket_id]" << std::endl;
    std::cerr << "    -id|--id socket_id : set the socket file name suffix, for testing only." << std::endl;

    return EXIT_FAILURE;
}

int main(int argc, char* argv[])
{
    int socketId = 0;
    if (argc != 1 && argc != 3)
        return usage();
    int i = 1;
    while (i < argc) {
        if ((strcmp(argv[i], "-id") == 0 || strcmp(argv[i], "--id") == 0) && i != (argc - 1)) {
            try {
                socketId = std::stoi(argv[++i]);
            } catch (std::exception& e) {
                return usage();
            }
        } else {
            return usage();
        }
        i++;
    }

    auto& pipeMgr = PipelineManager::getInstance();
    pipeMgr.init(socketId);

    auto& connector = XLinkConnector::getInstance();

    connector.init(pipeMgr);

    connector.run();
    connector.uninit();
    pipeMgr.uninit();

    return EXIT_SUCCESS;
}
