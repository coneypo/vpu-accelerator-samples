/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "../core/mediapipe_com.h"

static char *json_setup_elements(mediapipe_t *mp);

static char *
mp_element_block(mediapipe_t *mp, mp_command_t *cmd);

static mp_command_t  mp_element_commands[] = {
    {
        mp_string("element"),
        MP_MAIN_CONF,
        mp_element_block,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_core_module_t  mp_element_module_ctx = {
    mp_string("element"),
    NULL,
    NULL
};

mp_module_t  mp_element_module = {
    MP_MODULE_V1,
    &mp_element_module_ctx,                /* module context */
    mp_element_commands,                   /* module directives */
    MP_CORE_MODULE,                       /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    NULL,                    /* keyshot_process*/
    NULL,                               /* message_process */
    NULL,                      /* init_callback */
    NULL,                               /* netcommand_process */
    NULL,                               /* exit master */
    MP_MODULE_V1_PADDING
};

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis prase json config and set element property
 *
 * @Param mp
 */
/* ----------------------------------------------------------------------------*/
static char *
json_setup_elements(mediapipe_t *mp)
{
    struct json_object *parent, *ele, *ppty;
    struct json_object_iterator iter, end;
    const char *element_name = NULL, *ppty_name = NULL, *ppty_valuestring = NULL;
    enum json_type ppty_type;
    unsigned int len, i;
    int ret = -1;
    struct json_object *root = mp->config;
    if (!json_object_object_get_ex(root, "element", &parent))
        return MP_CONF_ERROR;
    if (!json_object_is_type(parent, json_type_array))
        return MP_CONF_ERROR;
    LOG_DEBUG("\n=====================begin config===================\n");
    len = json_object_array_length(parent);

    for (i = 0; i < len; ++i) {
        ele = json_object_array_get_idx(parent, i);
        end = json_object_iter_end(ele);
        iter = json_object_iter_begin(ele);

        if (json_object_iter_equal(&iter, &end)) {
            break;
        }

        if (0 != strcmp("name", json_object_iter_peek_name(&iter)))
            return MP_CONF_ERROR;
        ppty = json_object_iter_peek_value(&iter);
        if (!json_object_is_type(ppty, json_type_string))
            return MP_CONF_ERROR;
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
                    } else {      //fraction type
                        int a, b;

                        if (sscanf(ppty_valuestring, "%d/%d", &a, &b)) {
                            MEDIAPIPE_SET_PROPERTY(ret, mp, element_name, ppty_name, a, b, NULL);
                        }
                    }
                } else {          //caps
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
                /* GArray *array = g_array_sized_new (FALSE, TRUE, sizeof (GValue), 1); */
                /* g_array_set_clear_func (array, (GDestroyNotify) g_value_unset); */
                GValue v = { 0, };
                g_value_init(&v, G_TYPE_UCHAR);

                for (j = 0; j < l; ++j) {
                    jv = json_object_array_get_idx(ppty, j);
                    g_value_set_uchar(&v, json_object_get_int(jv));
                    g_value_array_append(array, &v);
                    /* g_array_append_val(array, v); */
                }

                MEDIAPIPE_SET_PROPERTY(ret, mp, element_name, ppty_name, array, NULL);
                g_value_array_free(array);
                /* g_array_unref(array); */
                break;
            }

            default:
                g_print("Unkown property type!\n");
                break;
            }

            json_object_iter_next(&iter);
        }
    }

    LOG_DEBUG("=====================end config====================\n\n");
    return MP_CONF_OK;
}

static char *
mp_element_block(mediapipe_t *mp, mp_command_t *cmd)
{
    return json_setup_elements(mp);
}



