/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "hddlchannel.h"
#include <QApplication>
#include <boost/program_options.hpp>
#include <memory>
#include <iostream>

using namespace boost::program_options;

struct Options {
    std::string pipeline;
    int index;
    int timeout;
};

std::shared_ptr<Options> parse_arguments(int argc, char* argv[])
{
    variables_map vm;
    options_description opt_desc("hddl channel options");
    opt_desc.add_options()("help,h", "This is hddl channel process program")(
        "pipeline,p", value<std::string>(), "[Required] config file path")(
        "index,i", value<int>(), "[Required]channel index")(
        "timeout,t", value<int>(), "timeout");

    store(parse_command_line(argc, argv, opt_desc), vm);
    notify(vm);

    if (vm.count("help")) {
        std::cerr<< opt_desc << std::endl;
        exit(EXIT_FAILURE);
    }

    auto options = std::make_shared<Options>();
    options->timeout = 0;

    if (vm.count("pipeline")) {
        options->pipeline= vm["pipeline"].as<std::string>();
    }
    if(vm.count("index")){
        options->index= vm["index"].as<int>();
    }
    if(vm.count("timeout")){
        options->timeout= vm["timeout"].as<int>();
    }
    return options;
}

int main(int argc, char* argv[])
{
    auto options = parse_arguments(argc, argv);
    if(!options){
        std::cerr<<"Parse arguments failed"<<std::endl;
        return EXIT_FAILURE;
    }
    QApplication a(argc, argv);

    HddlChannel w(options->index);
    w.initConnection();
    w.setupPipeline(QString::fromStdString(options->pipeline), "mysink");
    w.show();
    w.run(options->timeout);
    return a.exec();
}
