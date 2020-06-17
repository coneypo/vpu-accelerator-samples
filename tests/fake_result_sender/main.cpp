/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "utils/infermetasender.h"
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <regex>
#include <string>
#include <thread>
#include <vector>

using namespace HddlUnite;

static void print_usage(const char* program_name, int exit_code);

static const char* roi_simulation_file = nullptr;
static const char* socket_path = nullptr;

static bool parse_cmdline(int argc, char* argv[])
{
    const char* const brief = "hs:r:";
    const struct option details[] = {
        {
            "socket",
            1,
            nullptr,
            's',
        },
        {
            "roi",
            1,
            nullptr,
            'r',
        },
        {
            "help",
            0,
            nullptr,
            'h',
        },
        { nullptr, 0, nullptr, 0 }
    };

    int opt = 0;
    while (opt != -1) {
        opt = getopt_long(argc, argv, brief, details, nullptr);
        switch (opt) {
        case 's':
            socket_path = optarg;
            break;
        case 'r':
            roi_simulation_file = optarg;
            break;
        case 'h': /* help */
            print_usage(argv[0], 0);
            break;
        case '?': /* an invalid option. */
            print_usage(argv[0], 1);
            break;
        case -1: /* Done with options. */
            break;
        default: /* unexpected. */
            print_usage(argv[0], 1);
            abort();
        }
    }
    return true;
}

static void print_usage(const char* program_name, int exit_code)
{
    printf("Usage: %s...\n", program_name);
    printf(
        " -s --socket socket path.\n"
        " -r --roi roi simulation path.\n"
        "-h --help Display this usage information.\n");
    exit(exit_code);
}

int main(int argc, char* argv[])
{

    parse_cmdline(argc, argv);
    if (!roi_simulation_file || !socket_path) {
        std::cerr << "Invalid arguments" << std::endl;
        return EXIT_FAILURE;
    }
    std::string unix_domain_socket = socket_path;

    std::ifstream file(roi_simulation_file);

    if (!file.is_open()) {
        std::cerr << "can not open roi simulation file" << std::endl;
        return EXIT_FAILURE;
    }

    std::regex re("(\\d+),(\\d+),(\\d+),(\\d+),(\\w+),(\\d+),([\\d|\\.]+)");
    int x, y, w, h;
    size_t pts;
    std::string label, line;
    double prob;
    InferMetaSender client;
    if (client.connectServer(unix_domain_socket)) {
        while (std::getline(file, line)) {
            std::smatch result;
            if (std::regex_match(line, result, re)) {
                x = std::stoi(result[1]);
                y = std::stoi(result[2]);
                w = std::stoi(result[3]);
                h = std::stoi(result[4]);
                label = result[5];
                pts = std::stoul(result[6]);
                prob = std::stod(result[7]);
                client.serializeSave(x, y, w, h, label, pts, prob);
                client.send();
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            } else {
                std::cerr << "Invalidate data format:" << line << std::endl;
                return EXIT_FAILURE;
            }
        }
    } else {
        return -1;
    }
    return 0;
}
