/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2019 xiao <<user@hostname.org>>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-osdparser
 *
 * FIXME:Describe osdparser here.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "blendwrapper.h"
#include "gstmfxvideometa.h"
#include "gstosdparser.h"
#include "inferresultmeta.h"

#include <gst/gst.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY_STATIC(gst_osd_parser_debug);
#define GST_CAT_DEFAULT gst_osd_parser_debug

/* Filter signals and args */
enum {
    PROP_0
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */

#ifdef ENABLE_INTEL_VA_OPENCL
static const char sink_caps_str[] = GST_VIDEO_CAPS_MAKE("{ BGRA }") ";" GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:MFXSurface", "{ NV12 }");
static const char src_pic_caps_str[] = GST_VIDEO_CAPS_MAKE("{ BGRA } ") ";" GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:MFXSurface", "{ NV12 }");
#else
static const char sink_caps_str[] = GST_VIDEO_CAPS_MAKE("{ BGRA }");
static const char src_pic_caps_str[] = GST_VIDEO_CAPS_MAKE("{ BGRA } ");
#endif

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(sink_caps_str));
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS(src_pic_caps_str));

static gboolean gst_osd_parser_sink_event(GstPad* pad, GstObject* parent, GstEvent* event);
static GstFlowReturn gst_osd_parser_chain(GstPad* pad, GstObject* parent, GstBuffer* buf);
static void gst_osd_parser_finalize(GstOsdParser* parser);
static void gst_osd_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void gst_osd_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);

#define gst_osd_parser_parent_class parent_class
G_DEFINE_TYPE(GstOsdParser, gst_osd_parser, GST_TYPE_ELEMENT)

/* GObject vmethod implementations */
/* initialize the osdparser's class */
static void gst_osd_parser_class_init(GstOsdParserClass* klass)
{
    GObjectClass* gobject_class;
    GstElementClass* gstelement_class;

    gobject_class = (GObjectClass*)klass;
    gstelement_class = (GstElementClass*)klass;
    gobject_class->finalize = (GObjectFinalizeFunc)gst_osd_parser_finalize;

    gobject_class->set_property = gst_osd_set_property;
    gobject_class->get_property = gst_osd_get_property;

    gst_element_class_set_details_simple(gstelement_class, "OsdParser", "Osd blender", "Gst transform element", "xiao <<xiao.x.liu@@intel.com>>");

    gst_element_class_add_pad_template(gstelement_class, gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(gstelement_class, gst_static_pad_template_get(&sink_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void gst_osd_parser_init(GstOsdParser* filter)
{
    filter->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");
    GST_PAD_SET_PROXY_CAPS(filter->sinkpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);
    filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    GST_PAD_SET_PROXY_CAPS(filter->srcpad);
    gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

    gst_pad_set_event_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_osd_parser_sink_event));
    gst_pad_set_chain_function(filter->sinkpad, GST_DEBUG_FUNCPTR(gst_osd_parser_chain));

#ifdef ENABLE_INTEL_VA_OPENCL
    filter->blend_handle = cvdlhandler_create();
    filter->crop_handle = cvdlhandler_create();
#endif
}

static void gst_osd_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    (void)value;
    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_osd_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    (void)value;
    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/* GstElement vmethod implementations */
/* this function handles sink events */
static gboolean gst_osd_parser_sink_event(GstPad* pad, GstObject* parent, GstEvent* event)
{
    GstOsdParser* filter;
    gboolean ret;

    filter = GST_OSDPARSER(parent);

    GST_LOG_OBJECT(filter, "Received %s event: %" GST_PTR_FORMAT, GST_EVENT_TYPE_NAME(event), event);

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_CAPS: {
#ifdef ENABLE_INTEL_VA_OPENCL
        GstCaps* caps;
        gst_event_parse_caps(event, &caps);
        cvdlhandler_init(filter->blend_handle, caps, "GRAY8");
        cvdlhandler_init(filter->crop_handle, caps, "BGRA");
#endif
        ret = gst_pad_event_default(pad, parent, event);
        break;
    }
    default:
        ret = gst_pad_event_default(pad, parent, event);
        break;
    }
    return ret;
}

static void gst_osd_parser_clean(GstOsdParser* parser)
{
#ifdef ENABLE_INTEL_VA_OPENCL
    if (parser->blend_handle) {
        cvdlhandler_destroy(parser->blend_handle);
    }
    if (parser->crop_handle) {
        cvdlhandler_destroy(parser->crop_handle);
    }
    parser->blend_handle = NULL;
    parser->crop_handle = NULL;
#endif
}

static void gst_osd_parser_finalize(GstOsdParser* parser)
{
    // release pool
    gst_osd_parser_clean(parser);
    G_OBJECT_CLASS(parent_class)->finalize(G_OBJECT(parser));
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn gst_osd_parser_chain(GstPad* pad, GstObject* parent, GstBuffer* buf)
{
    (void)pad;
    GstOsdParser* filter = GST_OSDPARSER(parent);
    InferResultMeta* meta = gst_buffer_get_infer_result_meta(buf);
    if (meta) {
        blend(filter, buf, meta->boundingBox, meta->size, TRUE);
    }
    return gst_pad_push(filter->srcpad, buf);
}
