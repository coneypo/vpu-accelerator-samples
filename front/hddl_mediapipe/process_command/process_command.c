#include "mediapipe.h"

#define LAUNCH_KEY          "Launch"
#define CONFIG_KEY          "Config"


gboolean
create_pipeline(char *desc, mediapipe_t *mp)
{
    struct json_object *root = NULL;
    struct json_object *object = NULL;
    struct json_object *config_object = NULL;
    const char *launch = NULL;
    const char *config = NULL;
    
    root = json_create_from_string(desc);
    if (!root) {
        g_print("Failed to create json object!\n");
        return FALSE;
    }
 
    if (!(json_object_object_get_ex(root, "payload", &object) &&
                json_object_is_type(object, json_type_object))) {
        g_print("Failed to create pipeline: invalid payload from server!\n");
        json_destroy(&root);
        return FALSE;
    }
        
    // get launch data.
    if (!json_get_string(object, LAUNCH_KEY, &launch)) {
        g_print("Failed to parse launch command!\n");
        json_destroy(&root);
        return FALSE;
    }
        
    // get config data.
    if (!json_object_object_get_ex(object, CONFIG_KEY, &config_object)) {
        g_print("Failed to parse config command!\n");
        json_destroy(&root);
        return FALSE;
    }
    config = json_object_to_json_string(config_object);

    // create pipeline from string
    mediapipe_init_from_string(config, launch, mp);
    
    json_destroy(&root);
    return TRUE;
}


void
set_property(json_object *desc, mediapipe_t *mp)
{
    struct json_object *parent, *ele, *ppty;
    struct json_object_iterator iter, end;
    const char *element_name = NULL, *ppty_name = NULL, *ppty_valuestring = NULL;
    enum json_type ppty_type;
    unsigned int len, i;
    int ret = -1;
    struct json_object *root = desc;
    assert(json_object_object_get_ex(root, "payload", &parent));
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

}

