/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "Pipeline.h"
#include "mediapipe.h"
#include <gst/gst.h>
#include <thread>

namespace {

int setup_modules(mediapipe_t* mp)
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

gboolean set_property(const char* desc, mediapipe_t* mp)
{
    struct json_object *parent, *ele, *ppty;
    struct json_object_iterator iter, end;
    const char *element_name = NULL, *ppty_name = NULL, *ppty_valuestring = NULL;
    enum json_type ppty_type;
    unsigned int len, i;
    int ret = -1;
    struct json_object* root = json_create_from_string(desc);
    if (!json_object_object_get_ex(root, "property", &parent))
        return FALSE;
    if (!json_object_is_type(parent, json_type_array))
        return FALSE;
    len = json_object_array_length(parent);

    for (i = 0; i < len; ++i) {
        ele = json_object_array_get_idx(parent, i);
        end = json_object_iter_end(ele);
        iter = json_object_iter_begin(ele);

        if (json_object_iter_equal(&iter, &end)) {
            break;
        }

        if (0 != strcmp("name", json_object_iter_peek_name(&iter)))
            return FALSE;
        ppty = json_object_iter_peek_value(&iter);
        if (!json_object_is_type(ppty, json_type_string))
            return FALSE;
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
                    GstCaps* caps = gst_caps_from_string(ppty_valuestring);
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
                struct json_object* jv;
                GValueArray* array = g_value_array_new(1); //for scaling list
                GValue v = {
                    0,
                };
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

    return TRUE;
}
}

namespace hddl {

class Pipeline::Impl {
public:
    Impl(Pipeline* parent)
        : m_start(true)
        , m_mp(nullptr)
        , m_pipe(*parent)
    {
    }

    ~Impl() {}

    PipelineStatus create(std::string launch, std::string config)
    {
        m_mp = g_new0(mediapipe_t, 1);
        if (!m_mp)
            return PipelineStatus::ERROR;
        m_mp->pipe_id = m_pipe.m_id;

        m_mp->private_data = &m_pipe;
        m_mp->message_callback = (message_callback_t)(&Pipeline::Impl::pipeline_callback);

        auto ret = mediapipe_init_from_string(config.c_str(), launch.c_str(), m_mp);
        if (ret == FALSE)
            return PipelineStatus::INVALID_PARAMETER;

        if (setup_modules(m_mp) != 0)
            return PipelineStatus::INVALID_PARAMETER;

        return PipelineStatus::SUCCESS;
    }

    PipelineStatus modify(std::string config)
    {
        if (config.empty() || !set_property(config.c_str(), m_mp))
            return PipelineStatus::INVALID_PARAMETER;
        return PipelineStatus::SUCCESS;
    }

    PipelineStatus destroy()
    {
        stop();
        mediapipe_destroy(m_mp);
        return PipelineStatus::SUCCESS;
    }

    PipelineStatus play()
    {
        if (m_start) {
            m_starter = std::thread([this]() {
                mp_modules_parse_json_config(m_mp); // load config at second time to make setChannel take effect.
                mediapipe_start(m_mp);
            });
            m_start = false;
        } else {
            mediapipe_playing(m_mp);
        }
        return PipelineStatus::SUCCESS;
    }

    PipelineStatus stop()
    {
        mediapipe_stop(m_mp);
        if (m_starter.joinable()) {
            m_starter.join();
        }
        return PipelineStatus::SUCCESS;
    }

    PipelineStatus pause()
    {
        if (!mediapipe_pause(m_mp))
            return PipelineStatus::ERROR;
        return PipelineStatus::SUCCESS;
    }

    PipelineStatus setChannel(const std::string& element, const int channelId)
    {
        mediapipe_set_channelId(m_mp, element.c_str(), channelId);
        return PipelineStatus::SUCCESS;
    }

private:
    static void pipeline_callback(mediapipe_t* mp, GstMessage* msg)
    {
        auto pipe = (Pipeline*)mp->private_data;
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            pipe->setState(MPState::PIPELINE_EOS);
            pipe->sendEventToHost(PipelineEvent::PIPELINE_EOS);
        } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            pipe->setState(MPState::RUNTIME_ERROR);
            pipe->sendEventToHost(PipelineEvent::RUNTIME_ERROR);
        }
    }

    bool m_start;
    mediapipe_t* m_mp;
    Pipeline& m_pipe;
    std::thread m_starter;
};
}
