/* * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __CAIRO_H__
#define __CAIRO_H__

#include <gst/gst.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include "macros.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_CAIRO_RENDER_WIDTH 800
#define DEFAULT_CAIRO_RENDER_HEIGHT 200
#define FONT_SIZE_TEST_STRING "FONT"
typedef struct {
    cairo_surface_t *surface;
    cairo_t *cairo;
    PangoLayout *layout;
} cairo_render_t;

void
cairo_render_destroy(cairo_render_t *render);

cairo_render_t *
cairo_render_create(guint width, guint height, const char *font_desc);

gboolean
cairo_render_get_suggest_font_size(const char *font_desc, guint *width,
                                   guint *height);

gchar *
cairo_render_get_rgba_data(cairo_render_t *render, const char *text);

static GList *g_caior_render_list = 0;
static cairo_render_t *default_render = NULL;



#ifdef __cplusplus
}
#endif

#endif
