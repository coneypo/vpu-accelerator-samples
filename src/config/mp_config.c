/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mp_config.h"

#define DEFAULT_CONFIG_FILE_NAME0 "config.json"
#define DEFAULT_CONFIG_FILE_NAME1 "/etc/mediapipe/config.json"

#define DEFAULT_LAUNCH_FILE_NAME0 "launch.txt"
#define DEFAULT_LAUNCH_FILE_NAME1 "/etc/mediapipe/launch.txt"

const char *g_config_filename = 0;
const char *g_launch_filename = 0;

static const char *
get_version_string()
{
#if VER == VER_PREALPHA2
    return "prealpha2";
#elif VER == VER_ALPHA
    return "alpha";
#elif VER == VER_MASTER
    return "master";
#else
    return "unkown";
#endif
}

static void
print_usage(FILE *stream, const char *program_name, int exit_code)
{
    fprintf(stream, "Usage: %s [OPTION]...\n", program_name);
    fprintf(stream, "Version: %s\n", get_version_string());
    fprintf(stream,
            " -c --specify name of the config file.\n"
            " -l --specify name of the launch file.\n"
            " -h --help Display this usage information.\n");
    exit(exit_code);
}

gboolean
parse_cmdline(int argc, char *argv[])
{
    const char *const brief = "hc:l:";
    const struct option details[] = {
        { "config", 1, NULL, 'c'},
        { "launch", 1, NULL, 'l'},
        { "help", 0, NULL, 'h'},
        { NULL, 0, NULL, 0 }
    };
    int opt = 0;

    while (opt != -1) {
        opt = getopt_long(argc, argv, brief, details, NULL);

        switch (opt)    {
        case 'c':
            g_config_filename = optarg;
            break;

        case 'l':
            g_launch_filename = optarg;
            break;

        case 'h': /* -h or --help */
            print_usage(stdout, argv[0], 0);
            break;

        case '?': /* The user specified an invalid option. */
            print_usage(stderr, argv[0], 1);
            break;

        case -1: /* Done with options. */
            break;

        default: /* Something else: unexpected. */
            abort();
        }
    }

    g_config_filename = confirm_file(g_config_filename,
                                     DEFAULT_CONFIG_FILE_NAME0, DEFAULT_CONFIG_FILE_NAME1);

    if (g_config_filename) {
        g_print("Config file is %s\n", g_config_filename);
    } else {
        g_print("Can not find config file\n");
        return FALSE;
    }

    g_launch_filename = confirm_file(g_launch_filename,
                                     DEFAULT_LAUNCH_FILE_NAME0, DEFAULT_LAUNCH_FILE_NAME1);

    if (g_launch_filename) {
        g_print("Launch file is %s\n", g_launch_filename);
    } else {
        g_print("Can not find launch file\n");
        return FALSE;
    }

    return TRUE;
}


