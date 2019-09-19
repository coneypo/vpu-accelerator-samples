/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "jsonHandle.h"

gboolean
gapiosd_json_check_enable_state(struct json_object *parent, const char *enable_string)
{
    struct json_object *object = NULL;
    json_bool enable_value = FALSE;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(parent, enable_string, &object),
                       FALSE);
    RETURN_VAL_IF_FAIL(json_object_is_type(object, json_type_int)
                       || json_object_is_type(object, json_type_boolean), FALSE);
    enable_value = json_object_get_boolean(object);
    return enable_value;
}

gboolean
gapiosd_json_get_int(struct json_object *parent, const char *name, int *value)
{
    struct json_object *object = NULL;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(parent, name, &object), FALSE);
    RETURN_VAL_IF_FAIL(json_object_is_type(object, json_type_int), FALSE);
    *value = json_object_get_int(object);
    return TRUE;
}

gboolean
gapiosd_json_get_double(struct json_object *parent, const char *name, gdouble *value)
{
    struct json_object *object = NULL;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(parent, name, &object), FALSE);
    RETURN_VAL_IF_FAIL(json_object_is_type(object, json_type_double), FALSE);
    gdouble double_value = json_object_get_double(object);
    RETURN_VAL_IF_FAIL(double_value >= 0, FALSE);
    *value = double_value;
    return TRUE;
}

gboolean
gapiosd_json_get_uint(struct json_object *parent, const char *name, guint *value)
{
    struct json_object *object = NULL;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(parent, name, &object), FALSE);
    RETURN_VAL_IF_FAIL(json_object_is_type(object, json_type_int), FALSE);
    gint integer_value = json_object_get_int(object);
    RETURN_VAL_IF_FAIL(integer_value >= 0, FALSE);
    *value = integer_value;
    return TRUE;
}

const char *
gapiosd_json_get_string(struct json_object *parent, const char *name)
{
    struct json_object *object = NULL;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(parent, name, &object), NULL);
    RETURN_VAL_IF_FAIL(json_object_is_type(object, json_type_string), NULL);
    return (json_object_get_string(object));
}

gboolean
gapiosd_json_get_rgb(struct json_object *parent, const char *name, cv::Scalar *color)
{
    struct json_object *array = NULL;
    struct json_object *object = NULL;
    gint R = -1, G = -1, B = -1;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(parent, name, &array), FALSE);
    guint num_values = json_object_array_length(array);
    RETURN_VAL_IF_FAIL(num_values >= 3, FALSE);
    object = json_object_array_get_idx(array, 0);
    R = json_object_get_int(object);
    RETURN_VAL_IF_FAIL(R >= 0 && R <= 255, FALSE);
    object = json_object_array_get_idx(array, 1);
    G = json_object_get_int(object);
    RETURN_VAL_IF_FAIL(G >= 0 && G <= 255, FALSE);
    object = json_object_array_get_idx(array, 2);
    B = json_object_get_int(object);
    RETURN_VAL_IF_FAIL(B >= 0 && B <= 255, FALSE);
    *color = cv::Scalar(R, G, B);
    return TRUE;
}
