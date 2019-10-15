/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hddl_mediapipe.h"
#include "mp_branch.h"
#include "mp_config_json.h"
#include "mp_utils.h"

#include <gst/gst.h>

const char* g_config_file = "config_openvino_filesrc.json";
const char* g_launch_file = "launch_openvino_filesrc.txt";

static gchar* g_server_uri = NULL;
static gint g_pipe_id = 0;

static void
print_hddl_usage(const char* program_name, gint exit_code)
{
    g_print("Usage: %s...\n", program_name);
    g_print(
        "Create pipeline from server command:\n"
        " -u --specify uri of unix socket server.\n"
        " -i --specify id of unix socket client.\n"
        "Create pipeline from local file:\n"
        " -c --specify name of config file.\n"
        " -l --specify name of launch file.\n"
        " -h --help Display this usage information.\n");
    exit(exit_code);
}

static gboolean
parse_hddl_cmdline(int argc, char* argv[])
{
    const char* const brief = "hu:i:c:l:";
    const struct option details[] = {
        { "serveruri", 1, NULL, 'u' },
        {
            "clientid", 1, NULL, 'i',
        },
        {
            "config", 1, NULL, 'c',
        },
        {
            "launch", 1, NULL, 'l',
        },
        { "help", 0, NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int opt = 0;
    while (opt != -1) {
        opt = getopt_long(argc, argv, brief, details, NULL);
        switch (opt) {
        case 'u':
            g_server_uri = optarg;
            break;
        case 'i':
            g_pipe_id = atoi(optarg);
            break;
        case 'c':
            g_config_file = optarg;
            break;
        case 'l':
            g_launch_file = optarg;
            break;
        case 'h': /* help */
            print_hddl_usage(argv[0], 0);
            break;
        case '?': /* an invalid option. */
            print_hddl_usage(argv[0], 1);
            break;
        case -1: /* Done with options. */
            break;
        default: /* unexpected. */
            print_hddl_usage(argv[0], 1);
            abort();
        }
    }

    return TRUE;
}

static gboolean
handle_keyboard(GIOChannel* source, GIOCondition cond, gpointer data)
{
    mediapipe_t* mp = (mediapipe_t*)data;
    GstCaps* caps;
    char* str = NULL;
    int ret = 0;
    GValueArray* pLtArray = NULL;

    if (g_io_channel_read_line(source, &str, NULL, NULL,
            NULL)
        == G_IO_STATUS_NORMAL) {
        if (str[0] == 'q') {
            mediapipe_stop(mp);
            return TRUE;
        }

        if (str[0] == '?') {
            printf(" =========== mediapipe commands ==============================\n");
            mp_modules_keyshot_process(mp, "?");
            printf(" ===== 'q' : quit                                        =====\n");
            printf(" =============================================================\n");
            return TRUE;
        }

        ret = mp_modules_keyshot_process(mp, str);

        if (ret == MP_OK) {
            printf("Push command success\n");
        } else if (ret == MP_IGNORE) {
            printf("don't have this command\n");
            printf(" =========== mediapipe commands ==============================\n");
            mp_modules_keyshot_process(mp, "?");
            printf(" ===== 'q' : quit                                        =====\n");
            printf(" =============================================================\n");
        } else {
            printf("Push command failed\n");
        }
    }

    return TRUE;
}

static int setup_modules(mediapipe_t* mp)
{
    if (mp_preinit_modules() != MP_OK) {
        return MP_ERROR;
    }

    if (MP_OK != mp_create_modules(mp)) {
        printf("create_modules failed\n");
        return -1;
    }

    if (MP_OK != mp_modules_parse_json_config(mp)) {
        printf("modules_prase_json_config failed\n");
        return -1;
    }

    if (MP_OK != mp_init_modules(mp)) {
        printf("modules_init_modules failed\n");
        return -1;
    }

    if (MP_OK != mp_modules_init_callback(mp)) {
        printf("modules_init_callback failed\n");
        return -1;
    }

    return 0;
}

static int local_run(mediapipe_hddl_t* hp)
{
    GIOChannel* io_stdin;
    gchar* launch = NULL;
    gchar* config = NULL;
    gboolean ret = FALSE;

    if (!hp)
        return EXIT_FAILURE;

    // Create pipeline from local file.
    if ((launch = read_file(g_launch_file)) == NULL)
        return EXIT_FAILURE;
    if ((config = read_file(g_config_file)) == NULL)
        return EXIT_FAILURE;

    ret = mediapipe_init_from_string(config, launch, &hp->mp);
    g_free(launch);
    g_free(config);
    if (ret == FALSE)
        return EXIT_FAILURE;

    if (setup_modules(&hp->mp) != 0)
        return EXIT_FAILURE;

    io_stdin = g_io_channel_unix_new(fileno(stdin));
    g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, &hp->mp);

    mediapipe_start(&hp->mp);
    g_io_channel_unref(io_stdin);
    mediapipe_destroy(&hp->mp);

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
    parse_hddl_cmdline(argc, argv);

    gst_init(NULL, NULL);

    mediapipe_hddl_t* hp = NULL;

    if (g_server_uri == NULL) {
        hp = g_new0(mediapipe_hddl_t, 1);
        if (!hp)
            return EXIT_FAILURE;
        return local_run(hp);
    }

    if ((hp = hddl_mediapipe_setup(g_server_uri, g_pipe_id)) == NULL)
        return EXIT_FAILURE;

    if (setup_modules(&hp->mp) != 0)
        return EXIT_FAILURE;

    hddl_mediapipe_run(hp);
    hddl_mediapipe_destroy(hp);

    return EXIT_SUCCESS;
}
