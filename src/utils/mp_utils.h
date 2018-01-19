#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <unistd.h>

typedef enum {
    SLICE_TYPE_FAIL = -1,
    SLICE_TYPE_P,
    SLICE_TYPE_B,
    SLICE_TYPE_I,
    SLICE_TYPE_SP,
    SLICE_TYPE_SI,
} slice_type_t;

gchar *read_file(const char *filename);

gchar *fakebuff_create(guint32 color, gint width, gint height);

const char *get_local_ip_addr();

slice_type_t h264_get_slice_type(guint8 *data, gsize size);

slice_type_t h265_get_slice_type(guint8 *data, gsize size);

#define FILE_EXIST(file) (file && !access (file, F_OK))

#define confirm_file(file0, file1, file2) \
    (FILE_EXIST(file0) ? file0 : (FILE_EXIST(file1) ? file1 : (FILE_EXIST(file2) ? file2 : NULL)))

#ifdef __cplusplus
}
#endif

#endif
