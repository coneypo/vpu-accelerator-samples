#ifndef __MEDIAPIPE_BRANCH_H__
#define __MEDIAPIPE_BRANCH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mediapipe_com.h"

typedef struct {
    GstPad *probe_pad;
    gulong  probe_id;
} prob_item_t;

typedef struct mediapipe_branch_s  {
    guint input_width;
    guint input_height;
    GstElement *pipeline;
    GstElement *source;
    GList *probe_items;
    guint bus_watch_id;
    gboolean(*branch_init)(struct mediapipe_branch_s  *);
} mediapipe_branch_t;

void
gst_probe_item_destroy(gpointer data);

gboolean
gst_probe_list_append_new_item(GList *list, GstPad *pad, gulong id);

void
gst_probe_list_destroy(GList *list);

gboolean mediapipe_branch_push_buffer(mediapipe_branch_t  *branch,
                                      GstBuffer *buffer);

gboolean
mediapipe_setup_new_branch(mediapipe_t *mp, const gchar *element_name,
                           const gchar *pad_name, mediapipe_branch_t  *branch);

GstElement *
mediapipe_branch_create_pipeline(const gchar *pipeline_description);

gboolean
mediapipe_branch_start(mediapipe_branch_t  *branch);

void
mediapipe_branch_destroy_internal(mediapipe_branch_t  *branch);

gboolean
mediapipe_branch_reconfig(mediapipe_branch_t  *branch, guint width, guint height);

#ifdef __cplusplus
}
#endif

#endif
