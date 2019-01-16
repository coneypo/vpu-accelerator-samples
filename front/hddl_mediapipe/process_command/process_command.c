#include "mediapipe.h"
#include "../unixsocket/us_client.h"
#include "process_command.h"

static
void
set_property(char *desc, mediapipe_t *mp)
{
    struct json_object *parent, *ele, *ppty;
    struct json_object_iterator iter, end;
    const char *element_name = NULL, *ppty_name = NULL, *ppty_valuestring = NULL;
    enum json_type ppty_type;
    unsigned int len, i;
    int ret = -1;
    struct json_object *root = json_create_from_string(desc);
    assert(json_object_object_get_ex(root, "property", &parent));
    assert(json_object_is_type(parent, json_type_array));
    len = json_object_array_length(parent);

    for (i = 0; i < len; ++i) {
        ele = json_object_array_get_idx(parent, i);
        end = json_object_iter_end(ele);
        iter = json_object_iter_begin(ele);

        if (json_object_iter_equal(&iter, &end)) {
            break;
        }

        assert(0 == strcmp("name", json_object_iter_peek_name(&iter)));
        ppty = json_object_iter_peek_value(&iter);
        assert(json_object_is_type(ppty, json_type_string));
        element_name = json_object_get_string(ppty);
        json_object_iter_next(&iter);

        while (!json_object_iter_equal(&iter, &end)) {
            ppty_name = json_object_iter_peek_name(&iter);
            ppty = json_object_iter_peek_value(&iter);
            ppty_type = json_object_get_type(ppty);

            switch (ppty_type) {
            case json_type_string:
                ppty_valuestring = json_object_get_string(ppty);

                if (strcmp(ppty_name, "caps")) {
                    if (!strchr(ppty_valuestring, '/')) {
                        MEDIAPIPE_SET_PROPERTY(ret, mp, element_name, ppty_name, ppty_valuestring,
                                               NULL);
                    } else {
                        int a, b;

                        if (sscanf(ppty_valuestring, "%d/%d", &a, &b)) {
                            MEDIAPIPE_SET_PROPERTY(ret, mp, element_name, ppty_name, a, b, NULL);
                        }
                    }
                } else {
                    GstCaps *caps = gst_caps_from_string(ppty_valuestring);
                    MEDIAPIPE_SET_PROPERTY(ret, mp, element_name, "caps", caps, NULL);
                    gst_caps_unref(caps);
                }

                if (!ret) {
                    g_print("Set element: %s %s=%s\n", element_name, ppty_name, ppty_valuestring);
                }

                break;

            case json_type_int:
                MEDIAPIPE_SET_PROPERTY(ret, mp, element_name, ppty_name,
                                       json_object_get_int(ppty), NULL);

                if (!ret) {
                    g_print("Set element: %s %s=%d\n", element_name, ppty_name,
                            json_object_get_int(ppty));
                }

                break;

            case json_type_double:
                MEDIAPIPE_SET_PROPERTY(ret, mp, element_name, ppty_name,
                                       json_object_get_double(ppty), NULL);

                if (!ret) {
                    g_print("Set element: %s %s=%f\n", element_name, ppty_name,
                            json_object_get_double(ppty));
                }

                break;

            case json_type_array: {
                unsigned int l = json_object_array_length(ppty);
                unsigned int j;
                struct json_object *jv;
                GValueArray *array = g_value_array_new(1);      //for scaling list
                GValue v = { 0, };
                g_value_init(&v, G_TYPE_UCHAR);

                for (j = 0; j < l; ++j) {
                    jv = json_object_array_get_idx(ppty, j);
                    g_value_set_uchar(&v, json_object_get_int(jv));
                    g_value_array_append(array, &v);
                }

                MEDIAPIPE_SET_PROPERTY(ret, mp, element_name, ppty_name, array, NULL);
                g_value_array_free(array);
                break;
            }

            default:
                g_print("Unkown property type!\n");
                break;
            }

            json_object_iter_next(&iter);
        }
    }
    json_object_put(root);

}


/**
 * @Synopsis  process bus message
 *
 * @Param mp   mediapipe
 * @Param message
 *
 * @Returns
 */
gboolean
process_command(mediapipe_t *mp, void *message)
{
    GstMessage *m = (GstMessage *) message;
    const  GstStructure *s;
    int command_type;
    int payload_len;
    char *payload;
    if (GST_MESSAGE_TYPE(m) != GST_MESSAGE_APPLICATION) {
        return FALSE;
    }
    s = gst_message_get_structure(m);
    const gchar *name = gst_structure_get_name(s);

    if (g_strcmp0(name, "process_message") != 0) {
        return FALSE;
    }

    if (gst_structure_get(s,
                "command_type", G_TYPE_UINT, &command_type,
                "payload_len", G_TYPE_UINT, &payload_len,
                "payload", G_TYPE_STRING, &payload,
                NULL) == FALSE) {
        return FALSE;
    }

    switch(command_type) {
        case eCommand_Config:
        case eCommand_Launch:
             break;

        case eCommand_SetProperty:
             g_print("Receive set_property command from server.\n");
             if (payload_len != 0) {
                set_property(payload, mp);
             }
             break;

        case eCommand_PipeDestroy:
             g_print("Receive destroy pipeline command from server.\n");
             mediapipe_stop(mp);
             break;

        default:
             break;
    }

    return TRUE;
}

