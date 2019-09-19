/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __JSON_HANDLE_H__
#define __JSON_HANDLE_H__
#include <gst/gst.h>
#include <opencv2/gapi/render.hpp>
#include <json.h>

G_BEGIN_DECLS

#ifndef RETURN_VAL_IF_FAIL
#define RETURN_VAL_IF_FAIL(condition, value) \
    do{ \
        if (!(condition)) \
            return (value);  \
    }while(0)
#endif

gboolean gapiosd_json_get_int(struct json_object *parent, const char *name,
                      int *value);
gboolean gapiosd_json_get_uint(struct json_object *parent, const char *name,
                       guint *value);
const char *gapiosd_json_get_string(struct json_object *parent, const char *name);
gboolean gapiosd_json_get_double(struct json_object *parent, const char *name,
                         gdouble *value);
gboolean gapiosd_json_get_rgb(struct json_object *parent, const char *name,
                      cv::Scalar *color);
gboolean gapiosd_json_check_enable_state(struct json_object *parent,
                                 const char *enable_string);
G_END_DECLS

#endif /* __JSON_HANDLE_H__ */
