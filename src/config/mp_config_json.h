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

#ifndef __CONFIG_JSON_H__
#define __CONFIG_JSON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <json-c/json.h>
#include <string.h>
#include <assert.h>
#include "../utils/macros.h"
#include <gst/gst.h>

typedef gpointer(*config_func_t)(struct json_object *object);

struct json_object *json_create(const char *filename);

struct json_object *
json_create_from_string(const char *str);

void json_destroy(struct json_object **obj);

GList *
json_parse_config(struct json_object *root, const char *config_name,
                  config_func_t config_function);


gboolean
json_check_enable_state(struct json_object *parent, const char *enable_string);

gboolean
json_get_rgba(struct json_object *parent, const char *name, guint32 *color);

gboolean
json_get_string(struct json_object *parent, const char *name,
                const char **value);

gboolean
json_get_uint(struct json_object *parent, const char *name, guint *value);

gboolean
json_get_int(struct json_object *parent, const char *name, int *value);

#ifdef __cplusplus
}
#endif


#endif
