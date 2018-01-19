#include "mp_config_json.h"

#define ONE_BILLON_NANO_SECONDS 1000000000

static GList *g_malloc_list = 0;


struct json_object *
json_create(const char *filename)
{
    return json_object_from_file(filename);
}

void
json_destroy(struct json_object **obj)
{
    g_list_free_full(g_malloc_list, (GDestroyNotify)g_free);
    json_object_put(*obj);
    *obj = NULL;
}

gboolean
json_get_int(struct json_object *parent, const char *name, int *value)
{
    struct json_object *object = NULL;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(parent, name, &object), FALSE);
    RETURN_VAL_IF_FAIL(json_object_is_type(object, json_type_int), FALSE);
    *value = json_object_get_int(object);
    return TRUE;
}

gboolean
json_get_uint(struct json_object *parent, const char *name, guint *value)
{
    struct json_object *object = NULL;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(parent, name, &object), FALSE);
    RETURN_VAL_IF_FAIL(json_object_is_type(object, json_type_int), FALSE);
    gint integer_value = json_object_get_int(object);
    RETURN_VAL_IF_FAIL(integer_value >= 0, FALSE);
    *value = integer_value;
    return TRUE;
}

gboolean
json_get_string(struct json_object *parent, const char *name,
                const char **value)
{
    struct json_object *object = NULL;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(parent, name, &object), FALSE);
    RETURN_VAL_IF_FAIL(json_object_is_type(object, json_type_string), FALSE);
    *value = json_object_get_string(object);
    return TRUE;
}

gboolean
json_get_rgba(struct json_object *parent, const char *name, guint32 *color)
{
    struct json_object *array = NULL;
    struct json_object *object = NULL;
    gint R = -1, G = -1, B = -1, A = -1;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(parent, name, &array), FALSE);
    guint num_values = json_object_array_length(array);
    RETURN_VAL_IF_FAIL(num_values >= 4, FALSE);
    object = json_object_array_get_idx(array, 0);
    R = json_object_get_int(object);
    RETURN_VAL_IF_FAIL(R >= 0 && R <= 255, FALSE);
    object = json_object_array_get_idx(array, 1);
    G = json_object_get_int(object);
    RETURN_VAL_IF_FAIL(G >= 0 && G <= 255, FALSE);
    object = json_object_array_get_idx(array, 2);
    B = json_object_get_int(object);
    RETURN_VAL_IF_FAIL(B >= 0 && B <= 255, FALSE);
    object = json_object_array_get_idx(array, 3);
    A = json_object_get_int(object);
    RETURN_VAL_IF_FAIL(A > 0 && A <= 255, FALSE);
    *color = (A << 24) + (B << 16) + (G << 8) + R;
    return TRUE;
}

gboolean
json_check_enable_state(struct json_object *parent, const char *enable_string)
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






GList *
json_parse_config(struct json_object *root, const char *config_name,
                  config_func_t config_function)
{
    GList *list = NULL;
    struct json_object *object;
    struct json_object *object_array;
    RETURN_VAL_IF_FAIL(json_object_object_get_ex(root, config_name, &object_array),
                       NULL);
    int num_items =  json_object_array_length(object_array);

    for (int i = 0; i < num_items; ++i) {
        object = json_object_array_get_idx(object_array, i);
        gpointer param = config_function(object);

        if (param) {
            list = g_list_append(list, param);
        }
    }

    return list;
}






