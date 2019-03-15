/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mp_module.h"

#define MP_MAX_DYNAMIC_MODULES  128

mp_uint_t         mp_max_module;
static mp_uint_t  mp_modules_n;


mp_int_t
mp_preinit_modules(void)
{
    mp_uint_t  i;

    for (i = 0; mp_modules[i]; i++) {
        mp_modules[i]->index = i;
        mp_modules[i]->name = mp_module_names[i];
    }

    mp_modules_n = i;
    mp_max_module = mp_modules_n + MP_MAX_DYNAMIC_MODULES;
    return MP_OK;
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
    int i;
    void *ctx;
    mp_module_ctx_t *module;
    mp->modules = (mp_module_t **)malloc((mp_max_module + 1) * sizeof(mp_module_t *));
    if (mp->modules == NULL) {
        return MP_ERROR;
    }

    memset(mp->modules, 0x00, (mp_max_module + 1) * sizeof(mp_module_t *));

    mp->module_ctx = (void **)malloc(mp_max_module * sizeof(void *));
    if (mp->module_ctx == NULL) {
        return MP_ERROR;
    }

    memcpy(mp->modules, mp_modules,
           mp_modules_n * sizeof(mp_module_t *));
    mp->modules_n = mp_modules_n;

    for (i = 0; mp->modules[i]; i++) {
        if (mp->modules[i]->type != MP_CORE_MODULE) {
            continue;
        }

        module = mp->modules[i]->ctx;

        if (module->create_ctx) {
            ctx = module->create_ctx(mp);

            if (ctx == NULL) {
                return MP_ERROR;
            }

            mp->module_ctx[i] = ctx;
        }
    }

    for (i = 0; mp->modules[i]; i++) {
        if (mp->modules[i]->type != MP_CORE_MODULE) {
            continue;
        }

        module = mp->modules[i]->ctx;

        if (module->init_ctx) {
            if (module->init_ctx(mp->module_ctx[i]) == MP_CONF_ERROR) {
                printf("%s, init conf failed\n", mp->modules[i]->name);
                return MP_ERROR;
            }
        }
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

    free(mp->modules);
    free(mp->module_ctx);
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
