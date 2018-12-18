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
json_create_from_string(char *str);

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
