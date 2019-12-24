/*
 *Copyright (C) 2018 Intel Corporation
 *
 *SPDX-License-Identifier: LGPL-2.1-only
 *
 *This library is free software; you can redistribute it and/or modify it under the terms
 * of the GNU Lesser General Public License as published by the Free Software Foundation;
 * version 2.1.
 *
 *This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with this library;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include <gst/gst.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstosdparser.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
    if (!gst_element_register (plugin, "osdparser", GST_RANK_PRIMARY,
        GST_TYPE_OSDPARSER))
      return FALSE;

  return TRUE;
}

#ifndef VERSION
#define VERSION "1.0.0"
#endif
#ifndef PACKAGE
#define PACKAGE "GStreamer"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "GStreamer-osdparser"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://www.intel.com"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    osdparser,
    "OSD Parser filters for HDDL DEMO",
    plugin_init, VERSION,
    "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN);
