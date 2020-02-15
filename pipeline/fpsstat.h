#ifndef __FPS_STAT_H__
#define __FPS_STAT_H__

#include <atomic>
#include <chrono>
#include <gst/gst.h>
using namespace std::chrono;
class FpsStat {
public:
    FpsStat(GstPad* pad)
        : m_renderedFrames(0)
        , m_droppedFrames(0)
        , m_renderedFps(0)
        , m_droppedFps(0)
        , m_lastRendered(0)
        , m_lastDropped(0)
        , m_intervalInMs(100)
        , m_lastTimeStamp(system_clock::now())
    {
        m_pad = pad;
        probe();
    }

    inline void setDropFrames(uint64_t dropped)
    {
        if (dropped != -1)
            m_droppedFrames = dropped;
    }
    inline void setRenderFrames(uint64_t rendered)
    {
        if (rendered != -1)
            m_renderedFrames = rendered;
    }
    inline double getDropFps()
    {
        return m_droppedFps;
    }

    inline double getRenderFps()
    {
        return m_renderedFps;
    }

    inline void reset()
    {
        m_renderedFps = 0;
        m_droppedFps = 0;
        m_droppedFrames = 0;
        m_renderedFrames = 0;
        m_lastDropped = 0;
        m_lastRendered = 0;
        m_lastTimeStamp = system_clock::now();
    }

private:
    void probe();
    static GstPadProbeReturn
    on_video_sink_data_flow(GstPad* pad, GstPadProbeInfo* info,
        gpointer user_data);

    inline void calculate()
    {
        system_clock::time_point now = system_clock::now();
        int deltaMSec = duration_cast<milliseconds>(now - m_lastTimeStamp).count();
        if (deltaMSec < m_intervalInMs) {
            return;
        }

        uint64_t frameRendered = m_renderedFrames;
        uint64_t frameDropped = m_droppedFrames;
        double deltaSec = ((double)deltaMSec) / 1000;
        m_renderedFps = (double)(frameRendered - m_lastRendered) / deltaSec;
        m_droppedFps = (double)(frameDropped - m_lastDropped) / deltaSec;
        m_lastDropped = frameDropped;
        m_lastRendered = frameRendered;
        m_lastTimeStamp = now;
    }

private:
    GstPad* m_pad;
    std::atomic<uint64_t> m_renderedFrames;
    std::atomic<uint64_t> m_droppedFrames;
    double m_renderedFps;
    double m_droppedFps;
    uint64_t m_lastRendered;
    uint64_t m_lastDropped;
    int m_intervalInMs;
    system_clock::time_point m_lastTimeStamp;
};

#endif // __FPS_STAT_H__
