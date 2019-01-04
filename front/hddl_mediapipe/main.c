#include <string.h>
#include <stdio.h>

#include "mp_config_json.h"
#include "mp_branch.h"
#include "mp_utils.h"
#include "hddl_mediapipe.h"
#include "unixsocket/us_client.h"
#include "process_command/process_command.h"

const char *g_config_file = "config_openvino_filesrc.json";
const char *g_launch_file = "launch_openvino_filesrc.txt";

static gchar *g_server_uri = NULL;
static gint g_pipe_id = 0;

static void
print_hddl_usage(const char *program_name, gint exit_code)
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
parse_hddl_cmdline(int argc, char *argv[])
{
    const char *const brief = "hu:i:c:l:";
    const struct option details[] = {
        { "serveruri", 1, NULL, 'u'},
        { "clientid", 1, NULL, 'i',},
        { "config", 1, NULL, 'c',},
        { "launch", 1, NULL, 'l',},
        { "help", 0, NULL, 'h'},
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


static gboolean 
process_command(char *command_desc, mediapipe_hddl_t *hp)
{
    struct json_object *root = NULL;
    enum E_COMMAND_TYPE command_type = eCommand_None;
    gboolean continue_process = TRUE; 

    root = json_create_from_string(command_desc);
    if (!root) {
        g_print("Failed to create json object!\n");
        return FALSE;
    }

    if(!json_get_int(root, "type", &command_type)) {
        return FALSE;
    }

//    command_type = json_get_command_type(root);

    switch(command_type) {
        case eCommand_PipeCreate:
             g_print("Receive create_pipeline command from server.\n");
             if(hp->mp->state == STATE_NOT_CREATE) {
                create_pipeline(command_desc, hp->mp);
             }
             break;

        case eCommand_SetProperty:
             g_print("Receive set_property command from server.\n");
             set_property(root, hp->mp);
             break;

        case eCommand_PipeDestroy:
             g_print("Receive destroy pipeline command from server.\n");
             continue_process = FALSE;
             mediapipe_stop(hp->mp);
             break;

        default:
             break;
    }

    return continue_process;
}


static void *message_handler(void *data)
{
    mediapipe_hddl_t *hp = (mediapipe_hddl_t*)data;
    MessageItem *item = NULL;
    gboolean continue_process = TRUE;
    while(continue_process) {
        item = usclient_get_data(hp->client);
        continue_process = process_command(item->data, hp);
        usclient_free_item(item);
    }
    return NULL;
}


int main(int argc, char *argv[])
{
    GIOChannel *io_stdin;
    gchar *launch = NULL;
    gchar *config = NULL;
    gboolean ret = FALSE;
    
    parse_hddl_cmdline(argc, argv);
    
    mediapipe_hddl_t *hp = g_new0(mediapipe_hddl_t, 1);
    gst_init(&argc, &argv);

    mediapipe_t *mp = g_new0(mediapipe_t, 1);
    hp->mp = mp;
    hp->mp->state = STATE_NOT_CREATE;

    if(g_server_uri != NULL) {
        // Create pipeline from server command.
        hp->client = usclient_setup(g_server_uri, g_pipe_id);
        hp->pipe_id = g_pipe_id;
        hp->message_handle_thread = g_thread_new(NULL, message_handler, (gpointer)hp);

        int times = 2000;
        while ((hp->mp->state != STATE_READY) && times > 0) {
            g_usleep(1000);
            times--;
        }
        if (hp->mp->state != STATE_READY) {
            printf("Failed to create pipeline\n");
            return -1;
        }

    } else {
        // Create pipeline from local file.
        gchar *launch = read_file(g_launch_file);
        if(launch == NULL) {
            return -1;
        }
        gchar *config = read_file(g_config_file);
        if(config == NULL) {
            return -1;
        }
        ret = mediapipe_init_from_string(config, launch, hp->mp);
        g_free(launch);
        g_free(config);
        if(ret == FALSE) {
            return -1;
        }
    }


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
    usclient_destroy(hp->client);
    mediapipe_destroy(hp->mp);
    g_free(hp);
    return 0;
}
