/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __GST_BOUNDING_BOX_H__
#define __GST_BOUNDING_BOX_H__

#include <gst/gst.h>

#define MAX_STR_LEN 100

typedef struct BoundingBox {
    gint x;
    gint y;
    gint width;
    gint height;
    gchar* label;
    GstClockTime pts;
    gdouble probability;
    gfloat inferfps;
    gfloat decfps;
} BoundingBox;

#endif
