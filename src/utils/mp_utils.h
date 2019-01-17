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

#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <unistd.h>

typedef enum {
    SLICE_TYPE_FAIL = -1,
    SLICE_TYPE_P,
    SLICE_TYPE_B,
    SLICE_TYPE_I,
    SLICE_TYPE_SP,
    SLICE_TYPE_SI,
} slice_type_t;

void convert_from_byte_to_hex(const unsigned char* source, char* dest, int sourceLen);

void convert_from_hex_to_byte(const char* source, unsigned char* dest, int sourceLen);

gchar *read_file(const char *filename);

gboolean write_file(const gchar *data, const gchar *file_name);

gchar *fakebuff_create(guint32 color, gint width, gint height);

const char *get_local_ip_addr();

slice_type_t h264_get_slice_type(guint8 *data, gsize size);

slice_type_t h265_get_slice_type(guint8 *data, gsize size);

void check_config_value(char *data, const char *value);

#define FILE_EXIST(file) (file && !access (file, F_OK))

#define confirm_file(file0, file1, file2) \
    (FILE_EXIST(file0) ? file0 : (FILE_EXIST(file1) ? file1 : (FILE_EXIST(file2) ? file2 : NULL)))

#ifdef __cplusplus
}
#endif

#endif
