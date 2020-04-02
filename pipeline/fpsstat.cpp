#include "fpsstat.h"
#include "inferresultmeta.h"

GstPadProbeReturn
FpsStat::on_video_sink_data_flow(GstPad* pad, GstPadProbeInfo* info,
    gpointer user_data)
{
    FpsStat* obj = (FpsStat*)user_data;
    GstBuffer* buffer;
    GstMiniObject* mini_obj = (GstMiniObject*)GST_PAD_PROBE_INFO_DATA(info);
    if (GST_IS_BUFFER(mini_obj)) {
        obj->m_renderedFrames++;
        obj->calculate();

        buffer = GST_BUFFER(mini_obj);
        InferResultMeta* meta = gst_buffer_get_infer_result_meta(buffer);
        if (meta && meta->size > 0) {
            obj->m_decFps = meta->boundingBox[0].decfps;
            obj->m_inferFps = meta->boundingBox[0].inferfps;
        }
    }
    return GST_PAD_PROBE_OK;
}

void FpsStat::probe()
{
    gst_pad_add_probe((GstPad*)m_pad, GST_PAD_PROBE_TYPE_DATA_BOTH, on_video_sink_data_flow, this, nullptr);
}
