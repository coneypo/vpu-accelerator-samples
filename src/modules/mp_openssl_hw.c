/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mediapipe_com.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <openssl/pkcs12.h>
#include <openssl/engine.h>

mp_int_t mp_openssl_hw_init_module(mediapipe_t *mp);

static mp_command_t  mp_idr_commands[] = {
    {
        mp_string("openssl_hw"),
        MP_MAIN_CONF,
        NULL,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_module_ctx_t  mp_openssl_hw_module_ctx = {
    mp_string("openssl_hw"),
    NULL,
    NULL,
    NULL
};

mp_module_t  mp_openssl_hw_module = {
    MP_MODULE_V1,
    &mp_openssl_hw_module_ctx,                /* module context */
    NULL,                   /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                               /* init master */
    mp_openssl_hw_init_module,                               /* init module */
    NULL,                    /* keyshot_process*/
    NULL,                               /* message_process */
    NULL,                      /* init_callback */
    NULL,                               /* netcommand_process */
    NULL,                               /* exit master */
    MP_MODULE_V1_PADDING
};

mp_int_t mp_openssl_hw_init_module(mediapipe_t *mp)
{
    const EVP_CIPHER *cipher;
    int m;
    ENGINE_load_builtin_engines();
    ENGINE *afalg = NULL;
    const char *engine_id = "afalg";
    ENGINE_load_builtin_engines();
    afalg = ENGINE_by_id(engine_id);
	const char* id = ENGINE_get_id(afalg);
	LOG_INFO("Engine id %s\n", id);
    if (afalg) {
        if (ENGINE_init(afalg)) {
            // TODO: check ret value. If use ENGINE_METHOD_ALL, it can fail even if engine is registered for
            // algorithm we actually need
            ENGINE_set_default(afalg, ENGINE_METHOD_ALL); // TODO: enable only for particular algorithm(s)?
            ENGINE_finish(afalg);
        } else {
            ENGINE_free(afalg);
            afalg = NULL;
            LOG_ERROR("Could not init engine %s for OpenSSL\n", engine_id);
        }
    } else {
        LOG_ERROR("Could not get engine by id %s for OpenSSL\n", engine_id);
    }
    return MP_OK;
}


