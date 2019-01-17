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

#include <string.h>
#include <stdio.h>

#include "mp_config_json.h"
#include "mediapipe.h"
#include "mp_branch.h"
#include "mp_utils.h"

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

        if (str[0] == 'h') {
            MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "custom-aic-param",
                                   "1,1,1,1,1,1,1,1,20,", NULL);
            MEDIAPIPE_SET_PROPERTY(ret, mp, "src", "scene-mode", 7, NULL);
            printf("Push command success\n");
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
    mediapipe_t *mp;
    GIOChannel *io_stdin;

    if (mp_preinit_modules() != MP_OK) {
        return MP_ERROR;
    }

    if (!(mp = mediapipe_create(argc, argv))) {
        return -1;
    }

    if (MP_OK != mp_create_modules(mp)) {
        printf("create_modules falid\n");
        return -1;
    }

    if (MP_OK != mp_modules_prase_json_config(mp)) {
        printf("modules_prase_json_config falid\n");
        return -1;
    }

    if (MP_OK != mp_init_modules(mp)) {
        printf("modules_init_modules falid\n");
        return -1;
    }

    io_stdin = g_io_channel_unix_new(fileno(stdin));
    g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, mp);

    if (MP_OK != mp_modules_init_callback(mp)) {
        printf("modules_init_callback falid\n");
        return -1;
    }

    mediapipe_start(mp);
    g_io_channel_unref(io_stdin);
    mediapipe_destroy(mp);
    return 0;
}
