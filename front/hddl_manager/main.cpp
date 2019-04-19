#include <boost/program_options.hpp>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "PipelineManager.h"

#ifdef LOCAL_MODE
#include "LocalMode.h"
#else
#include "XLinkConnector.h"
#endif

using namespace hddl;
using namespace boost::program_options;

struct Options {
    int socketId;
    std::string localFile;
    int runSeconds;
};

static void print_usage(options_description& opt_desc)
{
    std::cerr << "Usage: hddl_manager [OPTION]" << std::endl;
    std::cerr << opt_desc << std::endl;
}

static std::shared_ptr<Options> parse_cmdline(int argc, char* argv[])
{
    variables_map vm;
    options_description opt_desc("options");

    opt_desc.add_options()("help,h", "This is hddl_manager program")("id,i", boost::program_options::value<int>()->default_value(0), "Set the socket file name suffix, for testing only")("local,l", boost::program_options::value<std::string>(), "Local mode, hddl_manager will launch pipelines according to the local config file")("time,t", boost::program_options::value<int>()->default_value(10), "Local mode, pipeline's run time in seconds");

    try {
        store(parse_command_line(argc, argv, opt_desc), vm);
        notify(vm);
    } catch (...) {
        print_usage(opt_desc);
        return {};
    }

    if (vm.count("help")) {
        print_usage(opt_desc);
        return {};
    }

    auto options = std::make_shared<Options>();

    if (vm.count("id"))
        options->socketId = vm["id"].as<int>();

    if (vm.count("local")) {
        options->localFile = vm["local"].as<std::string>();
        if (vm.count("time"))
            options->runSeconds = vm["time"].as<int>();
    }

    return options;
}

int main(int argc, char* argv[])
{
    auto options = parse_cmdline(argc, argv);
    if (!options)
        return EXIT_FAILURE;

    auto& pipeMgr = PipelineManager::getInstance();
    pipeMgr.init(options->socketId);

#ifdef LOCAL_MODE
    if (!options->localFile.empty()) {
        auto& local = LocalMode::getInstance();
        if (local.init(options->localFile, pipeMgr, options->runSeconds))
            return EXIT_FAILURE;

        local.run();

        pipeMgr.uninit();
        return EXIT_SUCCESS;
    } else {
        return EXIT_FAILURE;
    }
#else
    auto& connector = XLinkConnector::getInstance();

    connector.init(pipeMgr);

    connector.run();
    connector.uninit();
    pipeMgr.uninit();

    return EXIT_SUCCESS;
#endif
}
