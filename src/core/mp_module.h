/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef _MP_MODULE_H_INCLUDED_
#define _MP_MODULE_H_INCLUDED_


#ifdef __cplusplus
extern "C" {
#endif


#include "mediapipe_com.h"


#define mp_value_helper(n)   #n
#define mp_value(n)          mp_value_helper(n)
#define MP_MODULE_UNSET_INDEX  (mp_uint_t) -1
#define MP_MAIN_CONF  0x1


#define MP_MODULE_SIGNATURE_0                                                \
    mp_value(MP_PTR_SIZE) ","                                               \
    mp_value(MP_SIG_ATOMIC_T_SIZE) ","                                      \
    mp_value(MP_TIME_T_SIZE) ","

#if (MP_HAVE_KQUEUE)
#define MP_MODULE_SIGNATURE_1   "1"
#else
#define MP_MODULE_SIGNATURE_1   "0"
#endif

#if (MP_HAVE_IOCP)
#define MP_MODULE_SIGNATURE_2   "1"
#else
#define MP_MODULE_SIGNATURE_2   "0"
#endif

#if (MP_HAVE_FILE_AIO || MP_COMPAT)
#define MP_MODULE_SIGNATURE_3   "1"
#else
#define MP_MODULE_SIGNATURE_3   "0"
#endif

#if (MP_HAVE_AIO_SENDFILE || MP_COMPAT)
#define MP_MODULE_SIGNATURE_4   "1"
#else
#define MP_MODULE_SIGNATURE_4   "0"
#endif

#if (MP_HAVE_EVENTFD)
#define MP_MODULE_SIGNATURE_5   "1"
#else
#define MP_MODULE_SIGNATURE_5   "0"
#endif

#if (MP_HAVE_EPOLL)
#define MP_MODULE_SIGNATURE_6   "1"
#else
#define MP_MODULE_SIGNATURE_6   "0"
#endif

#if (MP_HAVE_KEEPALIVE_TUNABLE)
#define MP_MODULE_SIGNATURE_7   "1"
#else
#define MP_MODULE_SIGNATURE_7   "0"
#endif

#if (MP_HAVE_INET6)
#define MP_MODULE_SIGNATURE_8   "1"
#else
#define MP_MODULE_SIGNATURE_8   "0"
#endif

#define MP_MODULE_SIGNATURE_9   "1"
#define MP_MODULE_SIGNATURE_10  "1"

#if (MP_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
#define MP_MODULE_SIGNATURE_11  "1"
#else
#define MP_MODULE_SIGNATURE_11  "0"
#endif

#define MP_MODULE_SIGNATURE_12  "1"

#if (MP_HAVE_SETFIB)
#define MP_MODULE_SIGNATURE_13  "1"
#else
#define MP_MODULE_SIGNATURE_13  "0"
#endif

#if (MP_HAVE_TCP_FASTOPEN)
#define MP_MODULE_SIGNATURE_14  "1"
#else
#define MP_MODULE_SIGNATURE_14  "0"
#endif

#if (MP_HAVE_UNIX_DOMAIN)
#define MP_MODULE_SIGNATURE_15  "1"
#else
#define MP_MODULE_SIGNATURE_15  "0"
#endif

#if (MP_HAVE_VARIADIC_MACROS)
#define MP_MODULE_SIGNATURE_16  "1"
#else
#define MP_MODULE_SIGNATURE_16  "0"
#endif

#define MP_MODULE_SIGNATURE_17  "0"
#define MP_MODULE_SIGNATURE_18  "0"

#if (MP_HAVE_OPENAT)
#define MP_MODULE_SIGNATURE_19  "1"
#else
#define MP_MODULE_SIGNATURE_19  "0"
#endif

#if (MP_HAVE_ATOMIC_OPS)
#define MP_MODULE_SIGNATURE_20  "1"
#else
#define MP_MODULE_SIGNATURE_20  "0"
#endif

#if (MP_HAVE_POSIX_SEM)
#define MP_MODULE_SIGNATURE_21  "1"
#else
#define MP_MODULE_SIGNATURE_21  "0"
#endif

#if (MP_THREADS || MP_COMPAT)
#define MP_MODULE_SIGNATURE_22  "1"
#else
#define MP_MODULE_SIGNATURE_22  "0"
#endif

#if (MP_PCRE)
#define MP_MODULE_SIGNATURE_23  "1"
#else
#define MP_MODULE_SIGNATURE_23  "0"
#endif

#if (MP_HTTP_SSL || MP_COMPAT)
#define MP_MODULE_SIGNATURE_24  "1"
#else
#define MP_MODULE_SIGNATURE_24  "0"
#endif

#define MP_MODULE_SIGNATURE_25  "1"

#if (MP_HTTP_GZIP)
#define MP_MODULE_SIGNATURE_26  "1"
#else
#define MP_MODULE_SIGNATURE_26  "0"
#endif

#define MP_MODULE_SIGNATURE_27  "1"

#if (MP_HTTP_X_FORWARDED_FOR)
#define MP_MODULE_SIGNATURE_28  "1"
#else
#define MP_MODULE_SIGNATURE_28  "0"
#endif

#if (MP_HTTP_REALIP)
#define MP_MODULE_SIGNATURE_29  "1"
#else
#define MP_MODULE_SIGNATURE_29  "0"
#endif

#if (MP_HTTP_HEADERS)
#define MP_MODULE_SIGNATURE_30  "1"
#else
#define MP_MODULE_SIGNATURE_30  "0"
#endif

#if (MP_HTTP_DAV)
#define MP_MODULE_SIGNATURE_31  "1"
#else
#define MP_MODULE_SIGNATURE_31  "0"
#endif

#if (MP_HTTP_CACHE)
#define MP_MODULE_SIGNATURE_32  "1"
#else
#define MP_MODULE_SIGNATURE_32  "0"
#endif

#if (MP_HTTP_UPSTREAM_ZONE)
#define MP_MODULE_SIGNATURE_33  "1"
#else
#define MP_MODULE_SIGNATURE_33  "0"
#endif

#if (MP_COMPAT)
#define MP_MODULE_SIGNATURE_34  "1"
#else
#define MP_MODULE_SIGNATURE_34  "0"
#endif

#define MP_MODULE_SIGNATURE                                                  \
    MP_MODULE_SIGNATURE_0 MP_MODULE_SIGNATURE_1 MP_MODULE_SIGNATURE_2      \
    MP_MODULE_SIGNATURE_3 MP_MODULE_SIGNATURE_4 MP_MODULE_SIGNATURE_5      \
    MP_MODULE_SIGNATURE_6 MP_MODULE_SIGNATURE_7 MP_MODULE_SIGNATURE_8      \
    MP_MODULE_SIGNATURE_9 MP_MODULE_SIGNATURE_10 MP_MODULE_SIGNATURE_11    \
    MP_MODULE_SIGNATURE_12 MP_MODULE_SIGNATURE_13 MP_MODULE_SIGNATURE_14   \
    MP_MODULE_SIGNATURE_15 MP_MODULE_SIGNATURE_16 MP_MODULE_SIGNATURE_17   \
    MP_MODULE_SIGNATURE_18 MP_MODULE_SIGNATURE_19 MP_MODULE_SIGNATURE_20   \
    MP_MODULE_SIGNATURE_21 MP_MODULE_SIGNATURE_22 MP_MODULE_SIGNATURE_23   \
    MP_MODULE_SIGNATURE_24 MP_MODULE_SIGNATURE_25 MP_MODULE_SIGNATURE_26   \
    MP_MODULE_SIGNATURE_27 MP_MODULE_SIGNATURE_28 MP_MODULE_SIGNATURE_29   \
    MP_MODULE_SIGNATURE_30 MP_MODULE_SIGNATURE_31 MP_MODULE_SIGNATURE_32   \
    MP_MODULE_SIGNATURE_33 MP_MODULE_SIGNATURE_34


#define MP_MODULE_V1                                                         \
    MP_MODULE_UNSET_INDEX, MP_MODULE_UNSET_INDEX,                           \
    NULL, 0, 0, mp_version, MP_MODULE_SIGNATURE

#define MP_MODULE_V1_PADDING  0, 0, 0, 0, 0, 0, 0, 0

#define mp_null_command  { mp_null_string, 0, NULL, 0, 0, NULL }
#define mp_custom_command0(name) { mp_string("name"), MP_MAIN_CONF, NULL, 0, 0, NULL }

typedef struct {
    size_t      len;
    const char  *data;
} mp_str_t;

typedef struct {
    mp_str_t             name;
    void                 *(*create_ctx)(mediapipe_t *mp);
    char                 *(*init_ctx)(void *ctx);
    void                 (*destroy_ctx)(void *ctx);
} mp_module_ctx_t;

struct mp_command_s {
    mp_str_t             name;
    mp_uint_t            type;
    char                 *(*set)(mediapipe_t *mp, mp_command_t *cmd);
    mp_uint_t            conf;
    mp_uint_t            offset;
    void                 *post;
};

struct mp_module_s {
    mp_uint_t            ctx_index;
    mp_uint_t            index;

    char                 *name;

    mp_uint_t            spare0;
    mp_uint_t            spare1;

    mp_uint_t            version;
    const char           *signature;

    mp_module_ctx_t      *ctx;
    mp_command_t         *commands;
    mp_uint_t            type;

    mp_int_t (*init_master)(void);
    mp_int_t (*init_module)(mediapipe_t *mp);
    mp_int_t (*keyshot_process)(mediapipe_t *mp, void *userdata);
    mp_int_t (*message_process)(mediapipe_t *mp, void *message);
    mp_int_t (*init_callback)(mediapipe_t *mp);
    mp_int_t (*netcommand_process)(mediapipe_t *mp, void *command);
    void (*exit_master)(void);

    uintptr_t             spare_hook0;
    uintptr_t             spare_hook1;
    uintptr_t             spare_hook2;
    uintptr_t             spare_hook3;
    uintptr_t             spare_hook4;
    uintptr_t             spare_hook5;
    uintptr_t             spare_hook6;
    uintptr_t             spare_hook7;
};


mp_int_t
mp_preinit_modules(void);

mp_int_t
mp_create_modules(mediapipe_t *mp);

mp_int_t
mp_modules_prase_json_config(mediapipe_t *mp);

mp_int_t
mp_init_modules(mediapipe_t *mp);

mp_int_t
mp_modules_keyshot_process(mediapipe_t *mp,  char *str);

mp_int_t
mp_modules_message_process(mediapipe_t *mp,  GstMessage* msg);

mp_int_t
mp_modules_init_callback(mediapipe_t *mp);

void
mp_modules_exit_master(mediapipe_t *mp);

void *mp_modules_find_moudle_ctx(mediapipe_t *mp, const char *module_name);

extern mp_module_t  *mp_modules[];
extern mp_uint_t     mp_max_module;

extern char          *mp_module_names[];


#ifdef __cplusplus
}
#endif


#endif /* _MP_MODULE_H_INCLUDED_ */
