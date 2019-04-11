/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "mediapipe.h"
#include "mp_branch.h"
#include "mp_config_json.h"
#include "mp_utils.h"

static gboolean
handle_keyboard(GIOChannel* source, GIOCondition cond, gpointer data)
{
    mediapipe_t* mp = data;
    GstCaps* caps;
    char* str = NULL;
    int ret = 0;
    GValueArray* pLtArray = NULL;

    if (g_io_channel_read_line(source, &str, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {
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

        if (str[0] == 'h') {
            MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "custom-aic-param",
                "1,1,1,1,1,1,1,1,20,", NULL);
            MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "scene-mode", 7, NULL);
            printf("Push command success\n");
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

void* mp_thread(void* data)
{
    guint channel =*((guint*)data);
    if (mp_preinit_modules() != MP_OK) {
        return NULL;
    }

    const char* launchData = "videotestsrc ! video/x-raw, width=1920, height=1080 ! "
                             "vaapih264enc ! h264parse name=proc_src ! "
                             "video/x-h264,stream-format=byte-stream,alignment=au ! fakesink";
    char configData[100];
    sprintf(configData, "{\"xlink\":{\"channel\":%d}, \"element\":[]}", channel);
    g_print("str:%s\n", configData);

    mediapipe_t* mp = g_new0(mediapipe_t, 1);

    if (!mediapipe_init_from_string(configData, launchData, mp)) {
        return NULL;
    }

    if (MP_OK != mp_create_modules(mp)) {
        printf("create_modules failed\n");
        mediapipe_destroy(mp);
        return NULL;
    }

    if (MP_OK != mp_modules_prase_json_config(mp)) {
        printf("modules_prase_json_config failed\n");
        return NULL;
    }

    if (MP_OK != mp_init_modules(mp)) {
        printf("modules_init_modules failed\n");
        mediapipe_destroy(mp);
        return NULL;
    }

    if (MP_OK != mp_modules_init_callback(mp)) {
        printf("modules_init_callback failed\n");
        mediapipe_destroy(mp);
        return NULL;
    }

    mediapipe_start(mp);
    mediapipe_destroy(mp);

    return NULL;
}

int main(int argc, char* argv[])
{
#ifdef MULTI_THREAD_MODE
#define THREAD_NUM 3
    static guint channel[THREAD_NUM];
    for(int i=0;i<THREAD_NUM;i++){
        channel[i] = 1025 + i;
    }
    static GThread* a[THREAD_NUM];
    for(int i=0;i<THREAD_NUM;i++){
        a[i] = g_thread_new("test", mp_thread, &channel[i]);
    }
    for(int i=0;i<THREAD_NUM;i++){
        g_thread_join (a[i]);
    }
    return  0;
#endif

    if (mp_preinit_modules() != MP_OK) {
        return MP_ERROR;
    }

    const char* launchData = "videotestsrc ! video/x-raw, width=1920, height=1080 ! "
                             "vaapih264enc ! h264parse name=proc_src ! "
                             "video/x-h264,stream-format=byte-stream,alignment=au ! fakesink";
    const char* configData = "{}";

    mediapipe_t* mp = g_new0(mediapipe_t, 1);

    if (!mediapipe_init_from_string(configData, launchData, mp)) {
        return -1;
    }

    if (MP_OK != mp_create_modules(mp)) {
        printf("create_modules failed\n");
        mediapipe_destroy(mp);
        return -1;
    }

    if (MP_OK != mp_init_modules(mp)) {
        printf("modules_init_modules failed\n");
        mediapipe_destroy(mp);
        return -1;
    }

    GIOChannel* io_stdin = g_io_channel_unix_new(fileno(stdin));
    g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, mp);
    g_io_channel_unref(io_stdin);

    if (MP_OK != mp_modules_init_callback(mp)) {
        printf("modules_init_callback failed\n");
        mediapipe_destroy(mp);
        return -1;
    }

    mediapipe_start(mp);
    mediapipe_destroy(mp);

    return 0;
}
