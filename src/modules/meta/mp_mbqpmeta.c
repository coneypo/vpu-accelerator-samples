/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "mp_mbqpmeta.h"


GstVideoMBQP *
gst_video_mbqp_create(gint x, gint y, gint width, gint height, gint value)
{
    GstVideoMBQP *mbqp = g_new0(GstVideoMBQP, 1);
    mbqp->x = x;
    mbqp->y = y;
    mbqp->w = width;
    mbqp->h = height;
    mbqp->value = value;
    return mbqp;
}




