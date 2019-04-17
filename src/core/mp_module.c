/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mp_module.h"

#define MP_MAX_DYNAMIC_MODULES  128

mp_uint_t         mp_max_module;
static mp_uint_t  mp_modules_n;
static GHashTable* mp_module_table = NULL;

static void* mp_prepare(void* param)
{
    mp_module_table = g_hash_table_new(g_str_hash, g_str_equal);

    for (int i = 0; mp_modules[i]; i++) {
        mp_modules[i]->index = i;
        mp_modules[i]->name = mp_module_names[i];
        g_hash_table_insert(mp_module_table, mp_module_names[i], mp_modules[i]);
    }

    mp_modules_n = g_hash_table_size(mp_module_table);
    mp_max_module = mp_modules_n + MP_MAX_DYNAMIC_MODULES;

    return MP_OK;
}

mp_int_t
mp_preinit_modules(void)
{
    static GOnce my_once = G_ONCE_INIT;

    g_once(&my_once, mp_prepare, NULL);

    return (mp_int_t)my_once.retval;
}

static mp_module_t* mp_lookup_module(const char* module_name)
{
    return (mp_module_t*)g_hash_table_lookup(mp_module_table, module_name);
}

static mp_module_t* mp_lookup_module_by_short_name(const char* module_name)
{
    GString* full_name = g_string_new("mp_");

    g_string_append(full_name, module_name);
    g_string_append(full_name, "_module");

    mp_module_t* ret = (mp_module_t*)g_hash_table_lookup(mp_module_table, full_name->str);

    g_string_free(full_name, TRUE);

    return ret;
}

static GSList* mp_parse_module_list(struct json_object* root)
{
    if (!root) {
        return NULL;
    }

    struct json_object* modules = NULL;
    if (!json_object_object_get_ex(root, "module_list", &modules)) {
        LOG_ERROR("cannot find node \"module_list\"");
        return NULL;
    }

    int num_modules = json_object_array_length(modules);
    if (!num_modules) {
        LOG_ERROR("node \"module_list\" is empty");
        return NULL;
    }

    gboolean error = FALSE;
    GSList* module_list = NULL;
    GString* module_list_str = g_string_new("module_list: [ ");

    for (int i = 0; i < num_modules; ++i) {
        struct json_object* node = json_object_array_get_idx(modules, i);
        const char* module_name = json_object_get_string(node);
        gpointer module = mp_lookup_module_by_short_name(module_name);
        if (module) {
            module_list = g_slist_append(module_list, module);
            g_string_append(module_list_str, module_name);
            g_string_append_c(module_list_str, ' ');
        } else {
            LOG_ERROR("cannot find module named \"%s\"", module_name);
            error = TRUE;
            break;
        }
    }

    if (error) {
        g_string_free(module_list_str, TRUE);
        g_slist_free(module_list);
        return NULL;
    }

    g_string_append(module_list_str, "]");
    LOG_INFO("%s", module_list_str->str);
    g_string_free(module_list_str, TRUE);

    return module_list;
}

static gboolean mp_create_context(mediapipe_t* mp, int num_modules)
{
    for (int i = 0; i < num_modules; ++i) {
        if (mp->modules[i]->type != MP_CORE_MODULE) {
            continue;
        }

        mp_module_ctx_t* context = mp->modules[i]->ctx;

        if (context->create_ctx) {
            mp->module_ctx[i] = context->create_ctx(mp);
            if (!mp->module_ctx[i]) {
                LOG_ERROR("module \"%s\" failed to create context", mp->modules[i]->name);
                return FALSE;
            }
        }
    }

    return TRUE;
}

static gboolean mp_init_context(mediapipe_t* mp, int num_modules)
{
    for (int i = 0; i < num_modules; ++i) {
        if (mp->modules[i]->type != MP_CORE_MODULE) {
            continue;
        }

        mp_module_ctx_t* context = mp->modules[i]->ctx;

        if (context->init_ctx) {
            char* status = context->init_ctx(mp->module_ctx[i]);
            if (status == MP_CONF_ERROR) {
                LOG_ERROR("module \"%s\" failed to init context", mp->modules[i]->name);
                return FALSE;
            }
        }
    }

    return TRUE;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  create modules for mediapipe
 *
 * @Param mp
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
mp_int_t
mp_create_modules(mediapipe_t *mp)
{
    GSList* module_list = mp_parse_module_list(mp->config);
    if (!module_list) {
        LOG_ERROR("parse module list from config failed.");
        return MP_ERROR;
    }

    int num_modules = g_slist_length(module_list);

    mp->modules = g_new0(mp_module_t*, num_modules + 1);
    mp->module_ctx = g_new0(void*, num_modules);
    mp->modules_n = num_modules;

    for (int i = 0; i < num_modules; ++i) {
        mp->modules[i] = g_slist_nth_data(module_list, i);
    }

    if (!mp_create_context(mp, num_modules) || !mp_init_context(mp, num_modules)) {
        g_free(mp->modules);
        g_free(mp->module_ctx);
        mp->modules = NULL;
        mp->module_ctx = NULL;
        mp->modules_n = 0;
        return MP_ERROR;
    }

    return MP_OK;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis module prase json config and process
 *
 * @Param mp
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
mp_int_t
mp_modules_prase_json_config(mediapipe_t *mp)
{
    char           *rv;
    int i;
    mp_command_t *cmd;

    for (i = 0; mp->modules[i]; i++) {
        cmd = mp->modules[i]->commands;

        if (cmd == NULL) {
            continue;
        }

        for (/* void */; cmd->name.len; cmd++) {
            if (cmd->set) {
                rv = cmd->set(mp, cmd);

                if (rv == MP_CONF_OK) {
                    continue;
                } else if (rv == MP_CONF_ERROR) {
                    printf("cmd error:%s\n",cmd->name.data);
                    return MP_ERROR;
                } else {
                    printf("cmd : %s return error:%s\n ",cmd->name.data, rv);
                }
            }
        }
    }

    return MP_OK;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis modules to init modules
 *
 * @Param mp
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
mp_int_t
mp_init_modules(mediapipe_t *mp)
{
    mp_uint_t  i;

    for (i = 0; mp->modules[i]; i++) {
        if (mp->modules[i]->init_module) {
            if (mp->modules[i]->init_module(mp) != MP_OK) {
                printf("%s, init modules failed\n", mp->modules[i]->name);
                return MP_ERROR;
            }
        }
    }

    return MP_OK;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis modules to progress keyshot
 *
 * @Param mp
 * @Param str the keyboard char
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
mp_int_t
mp_modules_keyshot_process(mediapipe_t *mp,  char *str)
{
    mp_uint_t  i;
    mp_uint_t  ret;

    for (i = 0; mp->modules[i]; i++) {
        if (mp->modules[i]->keyshot_process) {
            ret = mp->modules[i]->keyshot_process(mp, str);

            if (ret == MP_OK) {
                return MP_OK;
            } else if (ret == MP_ERROR) {
                printf("%s, keyshot process error \n", mp->modules[i]->name);
                return MP_ERROR;
            } else  if (ret == MP_IGNORE) {
                continue;
            } else {
                printf("%s, keyshot process return value isn't valid : %d\n",
                       mp->modules[i]->name, (int)ret);
            }
        }
    }

    return MP_IGNORE;
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis modules add custom callback
 *
 * @Param mp
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
mp_int_t
mp_modules_init_callback(mediapipe_t *mp)
{
    mp_uint_t  i;

    for (i = 0; mp->modules[i]; i++) {
        if (mp->modules[i]->init_callback) {
            if (mp->modules[i]->init_callback(mp) != MP_OK) {
                printf("%s, init callback failed\n", mp->modules[i]->name);
                continue;
            }
        }
    }

    return MP_OK;
}


/* --------------------------------------------------------------------------*/
/**
 * @Synopsis  modules self Destroy
 *
 * @Param mp
 */
/* ----------------------------------------------------------------------------*/
void
mp_modules_exit_master(mediapipe_t *mp)
{
    mp_uint_t  i;

    if (!mp->modules)
        return;

    for (i = 0; mp->modules[i]; i++) {
        if (mp->modules[i]->exit_master) {
            mp->modules[i]->exit_master();
        }

        if (mp->modules[i]->type != MP_CORE_MODULE) {
            continue;
        }

        mp_module_ctx_t *module = mp->modules[i]->ctx;

        if (module->destroy_ctx)
            module->destroy_ctx(mp->module_ctx[i]);
    }

    g_free(mp->modules);
    g_free(mp->module_ctx);
    g_hash_table_destroy(mp_module_table);
}

/* --------------------------------------------------------------------------*/
/**
 * @Synopsis modules to progress GstMessage
 *
 * @Param mp
 * @Param msg GstMessage
 *
 * @Returns
 */
/* ----------------------------------------------------------------------------*/
mp_int_t
mp_modules_message_process(mediapipe_t *mp,  GstMessage* msg)
{
    mp_uint_t  i;
    mp_uint_t  ret;

    for (i = 0; mp->modules[i]; i++) {
        if (mp->modules[i]->message_process) {
            ret = mp->modules[i]->message_process(mp, msg);
            if (ret == MP_OK) {
                return MP_OK;
            } else if (ret == MP_ERROR) {
                printf("%s, msg process error \n", mp->modules[i]->name);
                return MP_ERROR;
            } else  if (ret == MP_IGNORE) {
                continue;
            } else {
                printf("%s, mgs process return value isn't valid : %d\n",
                       mp->modules[i]->name, (int)ret);
            }
        }
    }

    return MP_IGNORE;
}

void *mp_modules_find_moudle_ctx(mediapipe_t *mp, const char *module_name)
{
    for (mp_uint_t i = 0; mp->modules[i]; i++) {
        if (g_strcmp0(mp->modules[i]->ctx->name.data, module_name) == 0)
            return mp->module_ctx[i];
    }
    return NULL;
}