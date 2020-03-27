/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mediapipe_com.h"
#include <stdio.h>
//get message and progress it end

#define AES_256_GCM_IV_SIZE 12
#define AES_256_GCM_TAG_SIZE 16
#define MESSAGE_ENCRYPT_MAX_NUM 50


typedef struct {
    mediapipe_t *mp;
    size_t counter;
    const gchar *filepath;
    const gchar *ele_name;
    const gchar *sink_name;  
} encrypt_message_ctx;

typedef struct {
    encrypt_message_ctx msg_ctxs[MESSAGE_ENCRYPT_MAX_NUM];
    guint msg_ctx_num;  
} encrypt_ctx;


static mp_int_t init_module(mediapipe_t *mp);

static void *create_ctx(mediapipe_t *mp);

static void destroy_ctx(void *ctx);

static  mp_int_t
gva_write_hex_to_file_from_uint8_array(const char *file_path, const uint8_t *byte_field, unsigned int bytefield_length);

static void
dump_crypto_context(GstElement *gvaencrypt, guchar *iv, guchar *tag, encrypt_message_ctx *msg_ctxs);

static mp_command_t  mp_encrypt_commands[] = {
    {
        mp_string("encrypt"),
        MP_MAIN_CONF,
        NULL,
        0,
        0,
        NULL
    },
    mp_null_command
};

static mp_module_ctx_t  mp_encrypt_module_ctx = {
    mp_string("encrypt"),
    create_ctx,
    NULL,
    destroy_ctx
};

mp_module_t  mp_encrypt_module = {
    MP_MODULE_V1,
    &mp_encrypt_module_ctx,           /* module context */
    mp_encrypt_commands,              /* module directives */
    MP_CORE_MODULE,                    /* module type */
    NULL,                              /* init master */
    init_module,                              /* init module */
    NULL,                              /* keyshot_process*/
    NULL,                   /* message_process */
    NULL,                              /* init_callback */
    NULL,                              /* netcommand_process */
    NULL,                              /* exit master */
    MP_MODULE_V1_PADDING
};

static  mp_int_t
gva_write_hex_to_file_from_uint8_array(const char *file_path, const uint8_t *byte_field, unsigned int bytefield_length) {
    FILE *outfile;
    gchar *field;
    UNUSED(field);
    uint16_t zero = 0;
    if (file_path) {
        outfile = fopen(file_path, "wb");
        if (outfile) {
            for (size_t i = 0; i < bytefield_length; ++i) {
                uint32_t dec = (uint32_t)(byte_field[i]);
                if (dec < 16) {
                    fprintf(outfile,"%x", zero);
                    fprintf(outfile,"%x", (uint16_t)(byte_field[i]));
                }
                else {
                    fprintf(outfile,"%x", (uint16_t)(byte_field[i]));
                }
            }
            fclose(outfile);
            return MP_OK;
        }
    }
    return MP_ERROR;
}

/*static  mp_int_t
write_to_file(size_t size, guint8 *data) {
    unsigned char encrypt_data[size];
    FILE *outfile;

    GString *enc_data_path = g_string_new("/data/yangyang/file_");
    g_string_append_printf(enc_data_path, "%d", counter);
    g_string_append_printf(enc_data_path, ".enc");
    g_print("Part of the ecnrypted data is written to %s\n", enc_data_path->str);
    outfile = fopen(enc_data_path->str, "wb");
    memcpy(encrypt_data, data, size);
    fwrite(encrypt_data, sizeof(char), size, outfile);

    return MP_OK;
}*/

/*static GstFlowReturn
new_sample(GstElement *sink, mediapipe_t *mp) {
    GstSample *sample;
    GstFlowReturn status = GST_FLOW_ERROR;
    // Get buffer by signal
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (sample) {
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMemory *memory = gst_buffer_get_all_memory(buffer);
        GstMapInfo info;

        if (gst_memory_map(memory, &info, GST_MAP_READ) && write_to_file(info.size, info.data))
            status = GST_FLOW_OK;

        gst_memory_unmap(memory, &info);
        gst_memory_unref(memory);
        gst_sample_unref(sample);
    }
    return status;
}*/

static void *create_ctx(mediapipe_t *mp)
{
    encrypt_ctx *ctx = g_new0(encrypt_ctx, 1);
    if (!ctx) {
        return NULL;
    }
    return ctx;
}

static void destroy_ctx(void *_ctx)
{
    encrypt_ctx *ctx = (encrypt_ctx *)_ctx;
    g_free(ctx);
}

static void
dump_crypto_context(GstElement *gvaencrypt, guchar *iv, guchar *tag, encrypt_message_ctx *msg_ctxs) {
    int ret;
    UNUSED(ret);
    gchar *location;
    UNUSED(location);
    g_print("Dumping IV and tag\n");

    /*specify file to save iv value*/
    GString *iv_path = g_string_new(msg_ctxs->filepath);
    g_string_append_printf(iv_path, "iv_");
    g_string_append_printf(iv_path, "%s_", msg_ctxs->ele_name);
    g_string_append_printf(iv_path, "%ld", msg_ctxs->counter);
    g_string_append_printf(iv_path, ".pub");

    /*specify file to save tag value*/
    GString *tag_path = g_string_new(msg_ctxs->filepath);
    g_string_append_printf(tag_path, "tag_");
    g_string_append_printf(tag_path, "%s_", msg_ctxs->ele_name);
    g_string_append_printf(tag_path, "%ld", msg_ctxs->counter);
    g_string_append_printf(tag_path, ".pub");
    gva_write_hex_to_file_from_uint8_array(iv_path->str, iv, AES_256_GCM_IV_SIZE);
    gva_write_hex_to_file_from_uint8_array(tag_path->str, tag, AES_256_GCM_TAG_SIZE);

    /*specify file to save generated data*/
    GString *enc_data_path = g_string_new(msg_ctxs->filepath);
    g_string_append_printf(enc_data_path, "file_");
    g_string_append_printf(enc_data_path, "%s_", msg_ctxs->ele_name);
    g_string_append_printf(enc_data_path, "%ld", msg_ctxs->counter);
    g_string_append_printf(enc_data_path, ".enc");
    g_print("Part of the ecnrypted data is written to %s\n", enc_data_path->str);
    GstElement *sink = gst_bin_get_by_name(GST_BIN(msg_ctxs->mp->pipeline), msg_ctxs->sink_name);
    if (sink == NULL) {
        g_print("sink is null\n");
    }

    gst_element_set_state(sink, GST_STATE_NULL);
    MEDIAPIPE_SET_PROPERTY(ret, msg_ctxs->mp, msg_ctxs->sink_name, "location", enc_data_path->str, NULL);
    gst_element_set_state(sink, GST_STATE_PLAYING);

    g_print("IV path: %s\n", iv_path->str);
    g_print("Tag path: %s\n", tag_path->str);

    ++msg_ctxs->counter;
}

static mp_int_t
init_module(mediapipe_t *mp) {
    struct json_object *parent;
    struct json_object *root = mp->config;

    encrypt_ctx *ctx = (encrypt_ctx *) mp_modules_find_module_ctx(mp, "encrypt");
    if (!json_object_object_get_ex(root, "encrypt", &parent)) {
        LOG_WARNING("config json do not have encrypt block, use default '/data/', 'gvaencrypt', 'filesink'");
        ctx->msg_ctxs[0].filepath = "/data/";
        ctx->msg_ctxs[0].ele_name = "gvaencrypt";
        ctx->msg_ctxs[0].sink_name = "filesink";
        ctx->msg_ctxs[0].mp = mp;
        ctx->msg_ctx_num = 0;

        GstElement *encryption = gst_bin_get_by_name(GST_BIN(mp->pipeline), ctx->msg_ctxs[0].ele_name);
        if(encryption == NULL) {
            LOG_WARNING ("encryption is null\n");
            return MP_IGNORE;
        }
        g_signal_connect(encryption, "force_write", G_CALLBACK(dump_crypto_context), &ctx->msg_ctxs[0]);
        return MP_OK;
    };
    int i;
    int arraylen = json_object_array_length(parent);
    ctx->msg_ctx_num = arraylen;
    for(i=0; i<arraylen; i++) {
        struct json_object *encrpt_obj = json_object_array_get_idx(parent, i);
        if (!json_get_string(encrpt_obj, "filepath", &ctx->msg_ctxs[i].filepath)) {
            LOG_WARNING("change format config json do not have filepath, use default '/data.'");
            ctx->msg_ctxs[i].filepath = "/data/";
        }

        if (!json_get_string(encrpt_obj, "gvaencrypt", &ctx->msg_ctxs[i].ele_name)) {
            LOG_WARNING("change format config json do not have gvaencrypt, use default 'gvaencrypt'");
            ctx->msg_ctxs[i].ele_name = "gvaencrypt";
        }

        if (!json_get_string(encrpt_obj, "filesink", &ctx->msg_ctxs[i].sink_name)) {
            LOG_WARNING("change format config json do not have filesink, use default 'filesink'");
            ctx->msg_ctxs[i].sink_name = "filesink";
        }

        ctx->msg_ctxs[i].mp = mp;

        GstElement *encryption = gst_bin_get_by_name(GST_BIN(mp->pipeline), ctx->msg_ctxs[i].ele_name);
        if(encryption == NULL) {
            LOG_WARNING ("encryption is null\n");
            return MP_IGNORE;
        }
        g_signal_connect(encryption, "force_write", G_CALLBACK(dump_crypto_context), &ctx->msg_ctxs[i]);
    }

    return MP_OK;
}
