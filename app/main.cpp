/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "configparser.h"
#include "hddldemo.h"

#include <QApplication>
#include <boost/program_options.hpp>
#include <iostream>

using namespace boost::program_options;

struct Options {
    std::string configFile;
};

std::shared_ptr<Options> parse_arguments(int argc, char* argv[])
{
    variables_map vm;
    options_description opt_desc("hddl demo options");
    opt_desc.add_options()("help,h", "This is hddl demo program")(
        "config,c", value<std::string>(), "[Required] config file path");

    store(parse_command_line(argc, argv, opt_desc), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cerr << opt_desc << std::endl;
        exit(EXIT_FAILURE);
    }

    auto options = std::make_shared<Options>();

    if (vm.count("config")) {
        options->configFile = vm["config"].as<std::string>();
    }
    return options;
}

int main(int argc, char* argv[])
{
    auto options = parse_arguments(argc, argv);
    if (!options || !ConfigParser::instance()->loadConfigFile(options->configFile)) {
        return EXIT_FAILURE;
    }

    QApplication a(argc, argv);
    HDDLDemo w;
    //w.show();
    w.showFullScreen();

    return a.exec();
}
