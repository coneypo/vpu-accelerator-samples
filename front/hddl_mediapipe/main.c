#include <string.h>
#include <stdio.h>

#include "mp_config_json.h"
#include "mp_branch.h"
#include "mp_utils.h"
#include "hddl_mediapipe.h"

#define DEFAULT_LAUNCH_FILE "launch_openvino_filesrc.txt"
#define DEFAULT_CONFIG_FILE "config_openvino_filesrc.json"
#define DEFAULT_SERVER_URI  "us://127.0.0.1:9090"

static gchar *g_server_uri = DEFAULT_SERVER_URI;
static gint g_pipe_id = 0;


static void
print_hddl_usage(const char* program_name, gint exit_code)
{
    g_print("Usage: %s...\n", program_name);
    g_print(
            " -u --specify uri of unix socket server.\n"
            " -i --specify id of unix socket client.\n"
            " -h --help Display this usage information.\n");
    exit(exit_code);
}

static gboolean
parse_hddl_cmdline(int argc, char *argv[])
{
    const char* const brief = "hu:i:";
    const struct option details[] = {
        { "serveruri", 1, NULL, 'u'},
        { "clientid", 1, NULL, 'i',},
        { "help", 0, NULL, 'h'},
        { NULL, 0, NULL, 0 }
    };

    int opt = 0;
    while(opt != -1) {
        opt = getopt_long(argc, argv, brief, details, NULL);
        switch(opt) {
            case 'u':
                g_server_uri = optarg;
                break;
            case 'i':
                g_pipe_id = atoi(optarg);
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
    optind = 1;
    g_print("server uri is %s, pipe id is %d\n", g_server_uri, g_pipe_id);

    return TRUE;
}

static gboolean
handle_keyboard(GIOChannel *source, GIOCondition cond, gpointer data)
{
    mediapipe_t *mp = data;
    GstCaps *caps;
    char *str = NULL;
    int ret = 0;
    GValueArray *pLtArray = NULL;

    if (g_io_channel_read_line(source, &str, NULL, NULL,
                               NULL) == G_IO_STATUS_NORMAL) {
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

        ret =  mp_modules_keyshot_process(mp, str);

        if (ret == MP_OK) {
            printf("Push command success\n");
        } else if (ret == MP_IGNORE) {
            printf("don't have this command\n");
            printf(" =========== mediapipe commands ==============================\n");
            mp_modules_keyshot_process(mp, "?");
            printf(" ===== 'q' : quit                                        =====\n");
            printf(" =============================================================\n");
        } else {
            printf("Push command falid\n");
        }
    }

    return TRUE;
}

int main(int argc, char *argv[])
{
    GIOChannel *io_stdin;
    gchar      *launch_file = "/tmp/launch_file.txt";
    gchar      *config_file = "/tmp/config_file.json";

    parse_hddl_cmdline(argc, argv);

    mediapipe_hddl_t *hp = g_new0(mediapipe_hddl_t, 1);
    gst_init(&argc, &argv);

    //TODO create unix socket client
    //hp->us = usclient_setup(g_server_uri, g_pipe_id);
    //TODO get launch data and config data from server.
    //temporarily get from file for test.
    gchar *launch_data = read_file(DEFAULT_LAUNCH_FILE);
    if (launch_data == NULL) {
        return -1;
    }
    gchar *config_data = read_file(DEFAULT_CONFIG_FILE);
    if (config_data == NULL) {
        return -1;
    }

    gboolean ret = FALSE;
    ret = write_file(launch_data, launch_file);
    if (ret == FALSE) {
        g_free(launch_data);
        return -1;
    }
    ret = write_file(config_data, config_file);
    if (ret == FALSE) {
        g_free(config_data);
        return -1;
    }

    g_free(launch_data);
    g_free(config_data);

    if (!FILE_EXIST (launch_file) || !FILE_EXIST(config_file)) {
        printf ("file not exist\n");
        return -1;
    }

    gint tmp_argc = 5;
    gchar *tmp_argv[]= {argv[0], "-l", launch_file, "-c", config_file, NULL};
    mediapipe_t *tmp_mp = mediapipe_create(tmp_argc, tmp_argv);
    if (tmp_mp == NULL) {
        return -1;
    }
    hp->mp  = tmp_mp;
    hp->pipe_id = g_pipe_id;

    if (mp_preinit_modules() != MP_OK) {
        return MP_ERROR;
    }

    if (MP_OK != mp_create_modules(hp->mp)) {

        printf("create_modules falid\n");
        return -1;
    }

    if (MP_OK != mp_modules_prase_json_config(hp->mp)) {
        printf("modules_prase_json_config falid\n");
        return -1;
    }

    if (MP_OK != mp_init_modules(hp->mp)) {
        printf("modules_init_modules falid\n");
        return -1;
    }

    io_stdin = g_io_channel_unix_new(fileno(stdin));
    g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, hp->mp);


    if (MP_OK != mp_modules_init_callback(hp->mp)) {
        printf("modules_init_callback falid\n");
        return -1;
    }

    mediapipe_start(hp->mp);
    g_io_channel_unref(io_stdin);
    mediapipe_destroy(hp->mp);
    g_free(hp);
    return 0;
}
