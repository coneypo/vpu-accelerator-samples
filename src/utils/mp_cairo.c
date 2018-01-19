#include "mp_cairo.h"

void
cairo_render_destroy(cairo_render_t *render)
{
    RETURN_IF_FAIL(render != NULL);
    g_object_unref(render->layout);
    cairo_destroy(render->cairo);
    cairo_surface_destroy(render->surface);
}

//void
//cairo_render_cleanup ()
//{
//    if (g_caior_render_list) {
//        g_list_free_full (g_caior_render_list, (GDestroyNotify) cairo_render_destroy);
//        g_caior_render_list = NULL;
//    }
//
//    if (default_render) {
//        cairo_render_destroy (default_render);
//        default_render = NULL;
//    }
//}
//
cairo_render_t *
cairo_render_create(guint width, guint height, const char *font_desc)
{
    cairo_render_t *render = g_new0(cairo_render_t, 1);
    render->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width,
                      height);
    render->cairo = cairo_create(render->surface);
    render->layout = pango_cairo_create_layout(render->cairo);
    pango_layout_set_width(render->layout, width * PANGO_SCALE);
    pango_layout_set_height(render->layout, height * PANGO_SCALE);
    PangoFontDescription *description = pango_font_description_from_string(
                                            font_desc);
    pango_layout_set_font_description(render->layout, description);
    pango_font_description_free(description);
    cairo_set_source_rgb(render->cairo, 0.0, 0.0, 0.0);
    cairo_save(render->cairo);
    return render;
}

gboolean
cairo_render_get_suggest_font_size(const char *font_desc, guint *width,
                                   guint *height)
{
    PangoRectangle logical_rect;

    if (!default_render) {
        default_render = g_new0(cairo_render_t, 1);
        default_render->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                  DEFAULT_CAIRO_RENDER_WIDTH, DEFAULT_CAIRO_RENDER_HEIGHT);
        default_render->cairo = cairo_create(default_render->surface);
        default_render->layout = pango_cairo_create_layout(default_render->cairo);
    }

    PangoFontDescription *description = pango_font_description_from_string(
                                            font_desc);
    pango_layout_set_font_description(default_render->layout, description);
    pango_font_description_free(description);
    pango_layout_set_text(default_render->layout, FONT_SIZE_TEST_STRING, -1);
    pango_layout_get_pixel_extents(default_render->layout, NULL, &logical_rect);
    *width = logical_rect.width / strlen(FONT_SIZE_TEST_STRING);
    *height = logical_rect.height;
    return TRUE;
}

gchar *
cairo_render_get_rgba_data(cairo_render_t *render, const char *text)
{
    cairo_set_operator(render->cairo, CAIRO_OPERATOR_CLEAR);
    cairo_paint(render->cairo);
    cairo_restore(render->cairo);
    cairo_save(render->cairo);
    pango_layout_set_text(render->layout, text, -1);
    pango_cairo_show_layout(render->cairo, render->layout);
    guchar *data = cairo_image_surface_get_data(render->surface);
    return (gchar *) data;
}


