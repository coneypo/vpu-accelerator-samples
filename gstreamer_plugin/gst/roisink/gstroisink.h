/* GStreamer
 * Copyright (C) 2020 FIXME <fixme@example.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_ROISINK_H_
#define _GST_ROISINK_H_

#include "utils/infermetasender.h"
#include <gst/app/gstappsink.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_ROISINK (gst_roisink_get_type())
#define GST_ROISINK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_ROISINK, GstRoiSink))
#define GST_ROISINK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_ROISINK, GstRoiSinkClass))
#define GST_IS_ROISINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_ROISINK))
#define GST_IS_ROISINK_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_ROISINK))

typedef struct _GstRoiSink GstRoiSink;
typedef struct _GstRoiSinkClass GstRoiSinkClass;

struct _GstRoiSink {
    GstAppSink parent;
    gboolean isConnected;
    InferMetaSender* sender;
    const gchar* socketName;
};

struct _GstRoiSinkClass {
    GstAppSinkClass parentClass;
};

GType gst_roisink_get_type(void);

G_END_DECLS

#endif
