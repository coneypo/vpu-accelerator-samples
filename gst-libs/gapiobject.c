/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) YEAR AUTHOR_NAME AUTHOR_EMAIL
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
 * SECTION:element-plugin
 *
 * FIXME:Describe plugin here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! plugin ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "gapiobject.h"
#define g_api_object_parent_class parent_class
G_DEFINE_TYPE(GapiObject, g_api_object, G_TYPE_OBJECT);

/* GObject vmethod implementations */

/* initialize the plugin's class */
static void
g_api_object_class_init(GapiObjectClass *klass)
{
    /* GObjectClass* gobject_class; */
    /* gobject_class = (GObjectClass*) klass; */
}

/* initialize the new element
 * initialize instance structure
 */
static void
g_api_object_init(GapiObject *object)
{
}


gboolean render_sync(GstBuffer *outbuf, gpointer array)
{
    return TRUE;
}

GAPI_OBJECT_INFO gapi_info_map[] = {
    //{
    //   "text",
    //   gapiobjectText_get_type();
    //   gapiobjectText_create;
    //},
    //{
    //  ...
    //}
};



