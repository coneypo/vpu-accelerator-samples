/* * MIT License
 *
 * Copyright (c) 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
