#include "fpsstat.h"

GstPadProbeReturn
FpsStat::on_video_sink_data_flow (GstPad * pad, GstPadProbeInfo * info,
                         gpointer user_data)
{
    FpsStat* obj = (FpsStat *)user_data;
    GstMiniObject *mini_obj = (GstMiniObject *)GST_PAD_PROBE_INFO_DATA (info);
    if (GST_IS_BUFFER (mini_obj)) {
        obj->m_renderedFrames++;
        obj->calculate();
    }
    return GST_PAD_PROBE_OK;
}

void FpsStat::probe()
{
    gst_pad_add_probe((GstPad*)m_pad, GST_PAD_PROBE_TYPE_DATA_BOTH, on_video_sink_data_flow, this, nullptr);
}

