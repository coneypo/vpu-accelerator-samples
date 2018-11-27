#ifndef __GST_VIDEO_SEI_META_H__
#define __GST_VIDEO_SEI_META_H__

#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS
/**
    GstSEIMeta:
    @meta: parent #GstMeta
    @sei_type: GQuark describing the semantic of the SEI (f.i. a face, a pedestrian)
    @id: identifier of this particular SEI
    @parent_id: identifier of its parent SEI, used f.i. for SEI hierarchisation.
    @sei_payloads: a array for all the SEI data.

    Extra  buffer metadata describing an image supplemental enhancement information
*/
typedef struct {
    GstMeta    meta;
    GQuark     sei_type;
    gint       id;
    gint       parent_id;
    GPtrArray *sei_payloads;
} GstSEIMeta;

gboolean
gst_buffer_add_sei_meta(GstBuffer *buffer, const guint8 *data,
                        guint32 data_size);

GstSEIMeta *
gst_buffer_get_sei_meta(GstBuffer *buffer);

G_END_DECLS
#endif /* __GST_VIDEO_META_H__ */
