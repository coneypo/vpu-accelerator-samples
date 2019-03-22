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

#include "hddl_mediapipe_impl.h"
#include "message.h"

typedef struct _MessageItem {
    void* data;
    int len;
} MessageItem;

static void item_free_func(gpointer data)
{
    MessageItem* item = (MessageItem*)data;
    g_free(item->data);
    g_free(item);
}

static MessageItem* get_data_from_queue(mediapipe_hddl_impl_t* hp)
{
    if (!hp)
        return NULL;

    return g_async_queue_pop(hp->message_queue);
}

static gboolean wait_message_for(mediapipe_hddl_impl_t* hp, msg_type* type)
{
    if (!hp)
        return FALSE;

    while (1) {
        MessageItem* item = get_data_from_queue(hp);
        if (!item)
            continue;
        gboolean ret = wait_message(hp, item->data, item->len, type);
        item_free_func(item);
        if (!ret)
            continue;
        else
            return TRUE;
    }
}

static gboolean bus_callback(GstBus* bus, GstMessage* msg, gpointer data)
{
    int payload_len;
    char* payload;

    mediapipe_hddl_impl_t* hp = (mediapipe_hddl_impl_t*)data;

    if (GST_MESSAGE_TYPE(msg) != GST_MESSAGE_APPLICATION) {
        return TRUE;
    }
    const GstStructure* s = gst_message_get_structure(msg);
    const gchar* name = gst_structure_get_name(s);

    if (g_strcmp0(name, "process_message") != 0) {
        return TRUE;
    }

    if (gst_structure_get(s,
            "payload_len", G_TYPE_UINT, &payload_len,
            "payload", G_TYPE_POINTER, &payload,
            NULL)
        == FALSE) {
        return TRUE;
    }

    process_message(hp, payload, payload_len);

    g_free(payload);

    /**
     * we want to be notified again the next time there is a message
     * on the bus, so returning TRUE (FALSE means we want to stop watching
     * for messages on the bus and our callback should not be called again)
     **/
    return TRUE;
}

static void message_callback(void* private_data, void* data, int len)
{
    mediapipe_hddl_impl_t* hp = (mediapipe_hddl_impl_t*)private_data;

    if (!hp->is_running) {
        if (len != 0) {
            MessageItem* item = g_new0(MessageItem, 1);
            item->data = data;
            item->len = len;
            g_async_queue_push(hp->message_queue, item);
        }
    } else {
        GstStructure* s = gst_structure_new("process_message",
            "payload_len", G_TYPE_UINT, len,
            "payload", G_TYPE_POINTER, data,
            NULL);
        GstMessage* m = gst_message_new_application(NULL, s);
        gst_bus_post(hp->bus, m);
    }
}

mediapipe_hddl_t* hddl_mediapipe_setup(const char* server_uri, int pipe_id)
{
    mediapipe_hddl_impl_t* hp = g_new0(mediapipe_hddl_impl_t, 1);
    if (!hp)
        return NULL;

    mediapipe_hddl_t* hp_cast = (mediapipe_hddl_t*)hp;

    hp->hp.pipe_id = pipe_id;
    hp->hp.mp.xlink_channel_id = pipe_id;
    hp->is_running = FALSE;
    hp->is_mp_created = FALSE;

    hp->bus = gst_bus_new();
    if (!hp->bus)
        goto error;
    hp->bus_watch_id = gst_bus_add_watch(hp->bus, bus_callback, hp);

    hp->message_queue = g_async_queue_new_full(item_free_func);
    if (!hp->message_queue)
        goto error;

    hp->client = usclient_setup(server_uri, &message_callback, hp);
    if (!hp->client)
        goto error;

    if (!send_register(hp))
        goto error;

    // Wait for CREATE message received and mediapipe init done
    msg_type type = CREATE;
    if (!wait_message_for(hp, &type) || type == DESTROY)
        goto error;

    return hp_cast;

error:
    hddl_mediapipe_destroy(hp_cast);
    return NULL;
}

void hddl_mediapipe_run(mediapipe_hddl_t* hp)
{
    if (!hp)
        return;

    mediapipe_hddl_impl_t* hp_impl = (mediapipe_hddl_impl_t*)hp;

    // Wait for PLAY message received
    msg_type type = PLAY;
    if (!wait_message_for(hp_impl, &type) || type == DESTROY)
        return;

    hp_impl->is_running = TRUE;
    mediapipe_start(&hp->mp);
    hp_impl->is_running = FALSE;

    /*
     * Wait for DESTROY message received
     * If pipeline is running STOP won't reach here, it'll be sent to bus
     * If pipeline reaches EOS STOP will reach here and send ret_code(1) for now since it's not DESTROY
     */
    type = DESTROY;
    if (!wait_message_for(hp_impl, &type))
        return;
}

void hddl_mediapipe_destroy(mediapipe_hddl_t* hp)
{
    if (!hp)
        return;

    mediapipe_hddl_impl_t* hp_impl = (mediapipe_hddl_impl_t*)hp;

    usclient_destroy(hp_impl->client);

    if (hp_impl->bus)
        gst_object_unref(hp_impl->bus);
    g_source_remove(hp_impl->bus_watch_id);

    if (hp_impl->message_queue)
        g_async_queue_unref(hp_impl->message_queue);

    if (hp_impl->is_mp_created) {
        mediapipe_stop(&hp_impl->hp.mp);
        /*
         * mediapipe_destroy will free mediapipe_t pointer, because mediapipe_t is part of mediapipe_hddl_t, it's already
         * freed so it shouldn't be freed again.
         */
        mediapipe_destroy(&hp_impl->hp.mp);
    } else {
        g_free(hp_impl);
    }
}

void hddl_mediapipe_send_metadata(mediapipe_hddl_t* hp, const char* data, int len)
{
    mediapipe_hddl_impl_t* hp_impl = (mediapipe_hddl_impl_t*)hp;
    if (hp_impl->is_mp_created)
        send_metadata(hp_impl, data, len);
}
