/* * MIT License
 *
 * Copyright (c) 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "message.h"
#include "hddl_message.pb.h"
#include "us_client.h"
#include <arpa/inet.h>

using namespace hddl;

static gboolean set_property(const char* desc, mediapipe_t* mp)
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

static MsgRspType req_to_rsp(MsgReqType type)
{
    switch (type) {
    case CREATE_REQUEST:
        return CREATE_RESPONSE;
    case MODIFY_REQUEST:
        return MODIFY_RESPONSE;
    case DESTROY_REQUEST:
        return DESTROY_RESPONSE;
    case PLAY_REQUEST:
        return PLAY_RESPONSE;
    case STOP_REQUEST:
        return STOP_RESPONSE;
    case PAUSE_REQUEST:
        return PAUSE_RESPONSE;
    }

    // Make -Werror happy, flow will never reach here
    return DESTROY_RESPONSE;
}

static MsgReqType to_req_type(msg_type type)
{
    switch (type) {
    case CREATE:
        return CREATE_REQUEST;
    case DESTROY:
        return DESTROY_REQUEST;
    case MODIFY:
        return MODIFY_REQUEST;
    case PLAY:
        return PLAY_REQUEST;
    case PAUSE:
        return PAUSE_REQUEST;
    case STOP:
        return STOP_REQUEST;
    }

    // Make -Werror happy, flow will never reach here
    return DESTROY_REQUEST;
}

static void send_response(mediapipe_hddl_impl_t* hp, MsgResponse& response, bool async = true)
{
    std::string buf;
    if (!response.SerializeToString(&buf))
        return;

    if (async)
        usclient_send_async(hp->client, &buf[0], (int)buf.size());
    else
        usclient_send_sync(hp->client, &buf[0], (int)buf.size());
}

static gboolean check(mediapipe_hddl_impl_t* hp, const void* data, int len, MsgRequest& request, MsgResponse& response)
{
    if (!request.ParseFromArray(data, len))
        return TRUE;

    response.set_pipeline_id(hp->hp.pipe_id);
    response.set_req_seq_no(request.req_seq_no());
    response.set_ret_code(0);
    response.set_rsp_type(req_to_rsp(request.req_type()));

    if (request.pipeline_id() != hp->hp.pipe_id) {
        response.set_ret_code(1);
        send_response(hp, response);
        return FALSE;
    }

    return TRUE;
}

static gboolean handle(mediapipe_hddl_impl_t* hp, MsgRequest& request, MsgResponse& response)
{
    gboolean ret = TRUE;

    switch (request.req_type()) {
    case CREATE_REQUEST:
        if (mediapipe_init_from_string(request.create().config_data().c_str(),
                request.create().launch_data().c_str(), &hp->hp.mp)
            == FALSE) {
            response.set_ret_code(1);
            ret = FALSE;
        }
        hp->is_mp_created = TRUE;
        break;
    case DESTROY_REQUEST:
        if (hp->is_mp_created) {
            mediapipe_stop(&hp->hp.mp);
            // mediapipe will be destroyed in hddl_mediapipe_destroy
            // mediapipe_destroy(&hp->hp.mp);
        }
        break;
    case MODIFY_REQUEST:
        if (request.modify().config_data().empty() || !set_property(request.modify().config_data().c_str(), &hp->hp.mp)) {
            response.set_ret_code(1);
            ret = FALSE;
        }
        break;
    case PLAY_REQUEST:
        mediapipe_playing(&hp->hp.mp);
        break;
    case PAUSE_REQUEST:
        mediapipe_pause(&hp->hp.mp);
        break;
    case STOP_REQUEST:
        if (hp->is_running) {
            mediapipe_stop(&hp->hp.mp);
            hp->is_running = FALSE;
        } else {
            response.set_ret_code(1);
            ret = FALSE;
        }
        break;
    default:
        response.set_ret_code(1);
        ret = FALSE;
    }

    send_response(hp, response, response.rsp_type() != DESTROY_RESPONSE);

    return ret;
}

gboolean send_register(mediapipe_hddl_impl_t* hp)
{
    MsgResponse msg;
    msg.set_rsp_type(REGISTER_EVENT);
    msg.set_pipeline_id(hp->hp.pipe_id);

    send_response(hp, msg);
    return TRUE;
}

gboolean send_metadata(mediapipe_hddl_impl_t* hp, const char* data, int len)
{
    if (len <= 0)
        return FALSE;
    MsgResponse msg;
    msg.set_rsp_type(METADATA_EVENT);
    msg.set_pipeline_id(hp->hp.pipe_id);
    msg.mutable_metadata()->set_metadata(data, static_cast<size_t>(len));

    send_response(hp, msg);
    return TRUE;
}

gboolean wait_message(mediapipe_hddl_impl_t* hp, const void* data, int len, msg_type* type)
{
    MsgRequest request;
    MsgResponse response;

    if (type == NULL)
        return FALSE;

    if (!check(hp, data, len, request, response))
        return FALSE;

    if (request.req_type() == to_req_type(DESTROY))
        *type = DESTROY;

    if (request.req_type() != to_req_type(*type)) {
        response.set_ret_code(1);
        send_response(hp, response);
        return FALSE;
    }

    if (*type == PLAY) {
        send_response(hp, response);
        return TRUE;
    }

    return handle(hp, request, response);
}

gboolean process_message(mediapipe_hddl_impl_t* hp, const void* data, int len)
{
    MsgRequest request;
    MsgResponse response;

    if (!check(hp, data, len, request, response))
        return FALSE;

    return handle(hp, request, response);
}
