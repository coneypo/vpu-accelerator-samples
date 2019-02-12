/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <math.h>
#include <gst/base/gstbitreader.h>
#include <ctype.h>

#include "mp_utils.h"
#include "local_debug.h"

void
convert_from_byte_to_hex(const unsigned char* source, char* dest, int sourceLen)
{
    short i;
    unsigned char highbyte, lowbyte;

    for (i = 0; i < sourceLen; i++) {
        highbyte = source[i] >> 4;
        lowbyte = source[i] & 0x0f ;
        highbyte += 0x30;
        if (highbyte > 0x39)
            dest[i * 2] = highbyte + 0x07;
        else
            dest[i * 2] = highbyte;
        lowbyte += 0x30;
        if (lowbyte > 0x39)
            dest[i * 2 + 1] = lowbyte + 0x07;
        else
            dest[i * 2 + 1] = lowbyte;
    }
    return ;
}

void
convert_from_hex_to_byte(const char* source, unsigned char* dest, int sourceLen)
{
    short i;
    unsigned char highbyte, lowbyte;

    for (i = 0; i < sourceLen; i += 2) {
        highbyte = toupper(source[i]);
        lowbyte  = toupper(source[i + 1]);
        if (highbyte > 0x39)
            highbyte -= 0x37;
        else
            highbyte -= 0x30;

        if (lowbyte > 0x39)
            lowbyte -= 0x37;
        else
            lowbyte -= 0x30;

        dest[i / 2] = (highbyte << 4) | lowbyte;
    }
    return ;
}

gchar *
read_file(const char *filename)
{
    FILE *fp = NULL;

    if (!(fp = fopen(filename, "rt"))) {
        g_print("failed to open file: %s\n", filename);
        return NULL;
    }

    gchar *data = NULL;
    size_t data_size = 0;
    fseek(fp, 0, SEEK_END);
    data_size = ftell(fp);
    rewind(fp);
    data = g_new0(char, data_size + 1);

    if (data_size != fread(data, 1, data_size, fp)) {
        g_free(data);
        data = NULL;
    }

    fclose(fp);
    return data;
}

gboolean
write_file(const gchar *data, const gchar *file_name)
{
    FILE *fp = fopen(file_name, "w");
    if (fp == NULL) {
        g_print("Open file %s failed", file_name);
        return FALSE;
    }
    fwrite(data, 1, strlen(data), fp);
    fclose(fp);
    return TRUE;
}

gchar *
fakebuff_create(guint32 color, gint width, gint height)
{
    long total_bytes = sizeof(color) * width * height;
    gchar *buffer = (gchar *) g_malloc(total_bytes);
    memcpy(buffer, &color, sizeof(color));
    long head_bytes = sizeof(color);
    long copy_bytes = total_bytes - sizeof(color);

    while (copy_bytes) {
        if (copy_bytes <= head_bytes) {
            memcpy(buffer + head_bytes, buffer, copy_bytes);
            break;
        }

        memcpy(buffer + head_bytes, buffer, head_bytes);
        copy_bytes -= head_bytes;
        head_bytes += head_bytes;
    }

    return buffer;
}

static void get_local_net_name(char *szCardName)
{
    FILE *f;
    char line[100], *p, *c;
    f = fopen("/proc/net/route" ,"r");
    while(fgets(line, 100, f)) {
        p = strtok(line, " \t");
        c = strtok(NULL, " \t");

        if(p != NULL && c != NULL)
        {
            if(strcmp(c, "00000000") == 0)
            {
                strcpy(szCardName, p);
                break;
            }
        }
    }
    fclose(f);
}

const char *
get_local_ip_addr()
{
    int sock_fd;
    char *local_addr = NULL;
    struct sockaddr_in *addr;
    char szCardName[30] = {'\0'};
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
        struct ifreq ifr_ip;
        memset(&ifr_ip, 0, sizeof(ifr_ip));
        get_local_net_name(szCardName);
        strncpy(ifr_ip.ifr_name, szCardName, sizeof(ifr_ip.ifr_name) - 1);

        if (ioctl(sock_fd, SIOCGIFADDR, &ifr_ip) >= 0) {
            addr = (struct sockaddr_in *)&ifr_ip.ifr_addr;
            local_addr = inet_ntoa(addr->sin_addr);
        }

        close(sock_fd);
    }

    return (const char *)local_addr;
}

#define START_CODE_CHECK(data,i) \
    (data[i] == 0 && data[i+1] == 0 && data[i+2] == 0x01)
#define TRAIL_CODE_CHECK(data,i) \
    (data[i] == 0 && data[i+1] == 0 && data[i+2] == 0)
#define SKIP_N_BITS(reader, nbits) \
    gst_bit_reader_set_pos (reader, gst_bit_reader_get_pos (reader) + nbits)

static inline guint32
get_bits_value(GstBitReader *reader, guint nbits)
{
    if (!reader || !nbits) {
        return 0;
    }

    if (nbits <= 8) {
        guint8 res = 0;
        gst_bit_reader_get_bits_uint8(reader, &res, nbits);
        return res;
    } else if (nbits <= 16) {
        guint16 res = 0;
        gst_bit_reader_get_bits_uint16(reader, &res, nbits);
        return res;
    } else if (nbits <= 32) {
        guint32 res = 0;
        gst_bit_reader_get_bits_uint32(reader, &res, nbits);
        return res;
    } else if (nbits <= 64) {
        g_print("assertion 'nbits <= 32' failed\n");
        guint64 res = 0;
        gst_bit_reader_get_bits_uint64(reader, &res, nbits);
        return res;
    }

    return 0;
}

static gint
get_ue_golomb(GstBitReader *reader)
{
    gint leadingZeroBits = -1;
    guint readValue = 0;

    for (readValue = 0; !readValue; ++leadingZeroBits) {
        readValue = get_bits_value(reader, 1);
    }

    return ((0x01 << leadingZeroBits) - 1 + get_bits_value(reader,
            leadingZeroBits));
}

static gint
nal_to_rbsp(const guint8 *nal_buf, gint *nal_size, guint8 *rbsp_buf,
            gint *rbsp_size)
{
    gint i = 0, j = 0, count = 0;

    if (!nal_buf || !nal_size || *nal_size <= 0 || !rbsp_buf || !rbsp_size
        || *rbsp_size <= 0) {
        return -1;
    }

    for (i = 0; i < *nal_size; ++i) {
        if (count == 2) {
            // in NAL unit, 0x000000, 0x000001 or 0x000002 shall not occur at any byte-aligned position
            if (nal_buf[i] < 0x03) {
                return -1;
            }

            if (nal_buf[i] == 0x03) {
                // check the 4th byte after 0x000003, except when cabac_zero_word is used, in which case the last three bytes of this NAL unit must be 0x000003
                if ((i < *nal_size - 1) && (nal_buf[i + 1] > 0x03)) {
                    return -1;
                }

                if (i == *nal_size - 1) {
                    break;
                }

                ++i;
                count = 0;
            }
        }

        if (j >= *rbsp_size) {
            return -1;
        }

        rbsp_buf[j++] = nal_buf[i];

        if (nal_buf[i] == 0x00) {
            ++count;
        } else {
            count = 0;
        }
    }

    *nal_size = i;
    *rbsp_size = j;
    return j;
}

static int
find_nal_unit(guint8 *data, gint size, gint *nal_start, gint *nal_end)
{
    gint idx = 0;
    *nal_start = -1;
    *nal_end = -1;

    while (idx + 3 <= size) {
        if (START_CODE_CHECK(data, idx)) {
            idx += 3;
            *nal_start = idx;
            break;
        }

        ++idx;
    }

    if (nal_start < 0) {
        return 0;
    }

    while (idx + 3 <= size) {
        if (START_CODE_CHECK(data, idx) ||
            TRAIL_CODE_CHECK(data, idx)) {
            *nal_end = idx;
            break;
        }

        ++idx;
    }

    if (*nal_end < 0) {
        *nal_end = size;
    }

    return (*nal_start < 0) ? 0 : (*nal_end - *nal_start);
}

#define H264_NAL_UNIT_SLICE 0x01
#define H264_NAL_UNIT_IDR   0x05
#define H264_NAL_UNIT_SEI   0x06
#define H264_NAL_UNIT_SPS   0x07
#define H264_NAL_UNIT_PPS   0x08

static slice_type_t
h264_read_nal_unit(guint8 *buf, gint size)
{
    guint8 nal_unit_type = 0;
    gint slice_type = -1;
    GstBitReader *reader = gst_bit_reader_new(buf, size);
    SKIP_N_BITS(reader, 3);
    nal_unit_type = get_bits_value(reader, 5);

    if (nal_unit_type == H264_NAL_UNIT_SLICE ||
        nal_unit_type == H264_NAL_UNIT_IDR) {
        get_ue_golomb(reader);
        slice_type = get_ue_golomb(reader);
    }

    if (slice_type < 0 || slice_type > 9) {
        gst_bit_reader_free(reader);
        return SLICE_TYPE_FAIL;
    }

    gst_bit_reader_free(reader);
    return (slice_type_t)(slice_type % 5);
}

slice_type_t
h264_get_slice_type(guint8 *data, gsize size)
{
    slice_type_t slice_type = SLICE_TYPE_FAIL;

    if (!data || !size) {
        return SLICE_TYPE_FAIL;
    }

    guint8 *curr = data;
    gint left_bytes = size;
    gint nal_start, nal_end;

    while (find_nal_unit(curr, left_bytes, &nal_start, &nal_end)) {
        curr += nal_start;
        slice_type = h264_read_nal_unit(curr, nal_end - nal_start);

        if (slice_type != SLICE_TYPE_FAIL) {
            break;
        }

        curr += (nal_end - nal_start);
        left_bytes -= nal_end;
    }

#ifdef PRINT_SLICE_TYPE_INFO
    static guint offset = 0;
    g_print("[H264] %8lu: %s\n", (offset + size - left_bytes + nal_start),
            (slice_type == SLICE_TYPE_P)  ? "P Frame"  :
            ((slice_type == SLICE_TYPE_B)  ? "B Frame"  :
             ((slice_type == SLICE_TYPE_I)  ? "I Frame"  :
              ((slice_type == SLICE_TYPE_SP) ? "SP Frame" :
               ((slice_type == SLICE_TYPE_SI) ? "SI Frame" : "Unknown Slice Type")))));
    offset += size;
#endif
    return slice_type;
}

typedef struct {
    gint   pic_width_in_luma_samples;
    gint   pic_height_in_luma_samples;
    gint   log2_min_luma_coding_block_size_minus3;
    gint   log2_diff_max_min_luma_coding_block_size;
} H265SPS;

typedef struct {
    guint32 pps_seq_parameter_set_id;
    guint8  dependent_slice_segments_enabled_flag;
    guint8  num_extra_slice_header_bits;
} H265PPS;

typedef struct {
    guint    nal_unit_type;
    guint32  slice_type;
    H265SPS  sps_table[32];
    H265PPS  pps_table[256];
} CodedStream;

#define INVALID_SLICE_TYPE 0xFFFFFFFF

#define H265_NAL_UNIT_CODED_SLICE_RASL_R   0x09
#define H265_NAL_UNIT_CODED_SLICE_BLA_W_LP 0x10
#define H265_NAL_UNIT_CODED_SLICE_CRA      0x15
#define H265_NAL_UNIT_RESERVED_IRAP_VCL23  0x17
#define H265_NAL_UNIT_VPS                  0x20
#define H265_NAL_UNIT_SPS                  0x21
#define H265_NAL_UNIT_PPS                  0x22
#define H265_NAL_UNIT_PREFIX_SEI           0x27

#define ROUND_UP_N(num,align) ((((num) + ((align) - 1)) & ~((align) - 1)))

void h265_read_ptl(GstBitReader *reader, int max_sub_layers_minus1)
{
    SKIP_N_BITS(reader, 96);
    guint8 *sub_layer_profile_present_flag = g_new0(guint8, max_sub_layers_minus1);
    guint8 *sub_layer_level_present_flag = g_new0(guint8, max_sub_layers_minus1);

    for (gint i = 0; i < max_sub_layers_minus1; ++i) {
        sub_layer_profile_present_flag[i] = get_bits_value(reader, 1);
        sub_layer_level_present_flag[i]   = get_bits_value(reader, 1);
    }

    if (max_sub_layers_minus1 > 0) {
        for (gint i = max_sub_layers_minus1; i < 8; ++i) {
            SKIP_N_BITS(reader, 2);
        }
    }

    guint8 *sub_layer_profile_idc = g_new0(guint8, max_sub_layers_minus1);
    guint8 **sub_layer_profile_compatibility_flag = g_new0(guint8 *,
            max_sub_layers_minus1);

    for (gint i = 0; i < max_sub_layers_minus1; ++i) {
        sub_layer_profile_compatibility_flag[i] = g_new0(guint8, 32);
    }

    for (gint i = 0; i < max_sub_layers_minus1; ++i) {
        if (sub_layer_profile_present_flag[i]) {
            SKIP_N_BITS(reader, 3);
            sub_layer_profile_idc[i] = get_bits_value(reader, 5);

            for (int j = 0; j < 32; j++) {
                sub_layer_profile_compatibility_flag[i][j] = get_bits_value(reader, 1);
            }

            if (sub_layer_profile_idc[i] == 4 || sub_layer_profile_compatibility_flag[i][4]
                ||
                sub_layer_profile_idc[i] == 5 || sub_layer_profile_compatibility_flag[i][5] ||
                sub_layer_profile_idc[i] == 6 || sub_layer_profile_compatibility_flag[i][6] ||
                sub_layer_profile_idc[i] == 7 || sub_layer_profile_compatibility_flag[i][7]) {
                SKIP_N_BITS(reader, 48);
            } else {
                SKIP_N_BITS(reader, 49);
            }
        }

        if (sub_layer_level_present_flag[i]) {
            SKIP_N_BITS(reader, 8);
        }
    }

    g_free(sub_layer_profile_present_flag);
    g_free(sub_layer_level_present_flag);
    g_free(sub_layer_profile_idc);

    for (gint i = 0; i < max_sub_layers_minus1; ++i) {
        g_free(sub_layer_profile_compatibility_flag[i]);
    }

    g_free(sub_layer_profile_compatibility_flag);
}

void h265_read_sps_rbsp(CodedStream *stream, GstBitReader *reader)
{
    int sps_max_sub_layers_minus1 = 0;
    SKIP_N_BITS(reader, 4);
    sps_max_sub_layers_minus1 = get_bits_value(reader, 3);
    SKIP_N_BITS(reader, 1);
    h265_read_ptl(reader, sps_max_sub_layers_minus1);
    guint32 sps_seq_parameter_set_id = get_ue_golomb(reader);
    H265SPS *sps = &stream->sps_table[sps_seq_parameter_set_id];
    guint32 chroma_format_idc = get_ue_golomb(reader);

    if (chroma_format_idc == 3) {
        get_bits_value(reader, 1);
    }

    sps->pic_width_in_luma_samples  = get_ue_golomb(reader);
    sps->pic_height_in_luma_samples = get_ue_golomb(reader);
    guint8 conformance_window_flag = get_bits_value(reader, 1);

    if (conformance_window_flag) {
        get_ue_golomb(reader);
        get_ue_golomb(reader);
        get_ue_golomb(reader);
        get_ue_golomb(reader);
    }

    get_ue_golomb(reader);
    get_ue_golomb(reader);
    get_ue_golomb(reader);
    int i = (get_bits_value(reader, 1) ? 0 : sps_max_sub_layers_minus1);

    for (; i <= sps_max_sub_layers_minus1; i++) {
        get_ue_golomb(reader);
        get_ue_golomb(reader);
        get_ue_golomb(reader);
    }

    sps->log2_min_luma_coding_block_size_minus3   = get_ue_golomb(reader);
    sps->log2_diff_max_min_luma_coding_block_size = get_ue_golomb(reader);
}

void h265_read_pps_rbsp(CodedStream *stream, GstBitReader *reader)
{
    guint32 pps_pic_parameter_set_id = get_ue_golomb(reader);
    H265PPS *pps = &stream->pps_table[pps_pic_parameter_set_id];
    pps->pps_seq_parameter_set_id = get_ue_golomb(reader);
    pps->dependent_slice_segments_enabled_flag  = get_bits_value(reader, 1);
    get_bits_value(reader, 1);
    pps->num_extra_slice_header_bits = get_bits_value(reader, 3);
}

static void
h265_read_slice_header(CodedStream *stream, GstBitReader *reader)
{
    guint8 first_slice_segment_in_pic_flag = get_bits_value(reader, 1);

    if (stream->nal_unit_type >= H265_NAL_UNIT_CODED_SLICE_BLA_W_LP &&
        stream->nal_unit_type <= H265_NAL_UNIT_RESERVED_IRAP_VCL23) {
        get_bits_value(reader, 1);
    }

    guint32 slice_pic_parameter_set_id = get_ue_golomb(reader);
    H265PPS *pps = &stream->pps_table[slice_pic_parameter_set_id];
    H265SPS *sps = &stream->sps_table[pps->pps_seq_parameter_set_id];
    guint8 dependent_slice_segment_flag = 0;

    if (!first_slice_segment_in_pic_flag) {
        if (pps->dependent_slice_segments_enabled_flag) {
            dependent_slice_segment_flag = get_bits_value(reader, 1);
        }

        int maxCUWidth = 1 << (sps->log2_min_luma_coding_block_size_minus3 + 3 +
                               sps->log2_diff_max_min_luma_coding_block_size);
        int maxCUHeight = maxCUWidth;
        int numCTUs = ROUND_UP_N(sps->pic_width_in_luma_samples, maxCUWidth) *
                      ROUND_UP_N(sps->pic_height_in_luma_samples, maxCUHeight);
        int bitsSliceSegmentAddress = 0;

        while (numCTUs > (1 << bitsSliceSegmentAddress)) {
            ++bitsSliceSegmentAddress;
        }

        get_bits_value(reader, bitsSliceSegmentAddress);
    }

    if (!dependent_slice_segment_flag) {
        for (int i = 0; i < pps->num_extra_slice_header_bits; i++) {
            get_bits_value(reader, 1);
        }

        stream->slice_type = get_ue_golomb(reader);
    }

    return;
}

static slice_type_t
h265_read_nal_unit(guint8 *buf, gint size)
{
    slice_type_t slice_type = SLICE_TYPE_FAIL;
    gint rbsp_size = size;
    guint8 *rbsp_buf = g_new0(guint8, rbsp_size);

    if (nal_to_rbsp(buf, &size, rbsp_buf, &rbsp_size) < 0) {
        g_free(rbsp_buf);
        return SLICE_TYPE_FAIL;
    }

    CodedStream *stream = g_new0(CodedStream, 1);
    stream->slice_type = INVALID_SLICE_TYPE;
    GstBitReader *reader = gst_bit_reader_new(rbsp_buf, rbsp_size);
    get_bits_value(reader, 1);
    stream->nal_unit_type = get_bits_value(reader, 6);
    get_bits_value(reader, 6);
    get_bits_value(reader, 3);

    if (stream->nal_unit_type == H265_NAL_UNIT_SPS) {
        h265_read_sps_rbsp(stream, reader);
    } else if (stream->nal_unit_type == H265_NAL_UNIT_PPS) {
        h265_read_pps_rbsp(stream, reader);
    } else if (stream->nal_unit_type <= H265_NAL_UNIT_CODED_SLICE_RASL_R ||
               (stream->nal_unit_type >= H265_NAL_UNIT_CODED_SLICE_BLA_W_LP &&
                stream->nal_unit_type <= H265_NAL_UNIT_CODED_SLICE_CRA)) {
        h265_read_slice_header(stream, reader);
    }

    if (stream->slice_type == 0) {
        slice_type = SLICE_TYPE_B;
    } else if (stream->slice_type == 1) {
        slice_type = SLICE_TYPE_P;
    } else if (stream->slice_type == 2) {
        slice_type = SLICE_TYPE_I;
    }

    gst_bit_reader_free(reader);
    g_free(rbsp_buf);
    g_free(stream);
    return slice_type;
}

slice_type_t
h265_get_slice_type(guint8 *data, gsize size)
{
    slice_type_t slice_type = SLICE_TYPE_FAIL;

    if (!data || !size) {
        return SLICE_TYPE_FAIL;
    }

    guint8 *curr = data;
    gint left_bytes = size;
    gint nal_start, nal_end;

    while (find_nal_unit(curr, left_bytes, &nal_start, &nal_end)) {
        curr += nal_start;
        slice_type = h265_read_nal_unit(curr, nal_end - nal_start);

        if (slice_type != SLICE_TYPE_FAIL) {
            break;
        }

        curr += (nal_end - nal_start);
        left_bytes -= nal_end;
    }

#ifdef PRINT_SLICE_TYPE_INFO
    static guint offset = 0;
    g_print("[H265] %8lu: %s\n", (offset + size - left_bytes + nal_start),
            (slice_type == SLICE_TYPE_P)  ? "P Frame"  :
            ((slice_type == SLICE_TYPE_B)  ? "B Frame"  :
             ((slice_type == SLICE_TYPE_I)  ? "I Frame"  :
              ((slice_type == SLICE_TYPE_SP) ? "SP Frame" :
               ((slice_type == SLICE_TYPE_SI) ? "SI Frame" : "Unknown Slice Type")))));
    offset += size;
#endif
    return slice_type;
}
void check_config_value(char *data, const char *value)
{
    if(data[0] == '\0') {
        if(value != NULL) {
            strcpy(data, value);
            LOG_WARNING("fail to get value, use default value \"%s\"", data);
        }
    }
}
