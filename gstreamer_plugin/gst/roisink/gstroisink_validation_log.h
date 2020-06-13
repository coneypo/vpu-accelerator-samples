/* GStreamer
 * Copyright (C) 2020 FIXME <fixme@example.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_ROISINK_VALIDATION_LOG_H_
#define _GST_ROISINK_VALIDATION_LOG_H_

#include <gst/app/gstappsink.h>
#include <gst/base/gstbasesink.h>
#include <gst/gst.h>
#include <gst/video/gstvideometa.h>

#include <fstream>
#include <cstdlib>
#include <string.h>

typedef struct _LogStruct LogStruct;
struct _LogStruct {
    std::ofstream m_of;
};

inline void* init_validation_log() {
    static bool g_inited =false;
    static LogStruct * plogHandle = NULL;
    static const char* logName = "ValidationDump.csv";

    if(g_inited) {
        return plogHandle;
    }

    if(const char* env_p = std::getenv("VALIDATION_DUMP")) {
        if(strcmp(env_p, "1") != 0) {
            g_inited = true;
            std::cout << "Got env: 'VALIDATION_DUMP != 1', disable dump log" << std::endl;
            return NULL;
        }
        else {
            std::cout << "Got env: VALIDATION_DUMP = " << env_p << std::endl;
        }
    }
    else {
        g_inited = true;
        std::cout << "Couldn't get env: VALIDATION_DUMP , disable dump log" << std::endl;
        return NULL;
    }

    plogHandle = new LogStruct();
    if(NULL == plogHandle) {
        std::cout << "Error: can't new" << std::endl;
        return NULL;
    }

    plogHandle->m_of.open(logName);
    if(!plogHandle->m_of.is_open()){
        std::cout<<"Error opening " << logName << " file"<<std::endl;
        return NULL;
    }
    plogHandle->m_of << "ts.ia.start, ts.ia.end, ts.xbay.start, ts.xbay.end, streamId, frameId, roi.x, roi.y, roi.width, roi.height, "<<
            "trackingId, detId, detProb, num_cls, nn_name, clsLabel, clsProb"<<std::endl;

    g_inited = true;
    
    return plogHandle;
}

inline void dump_one_frame_log(void* handle, GstVideoRegionOfInterestMeta* meta) {
    int cls_label_id = 0;
    int det_label_id = 0;
    int object_id = 0;
    int cls_prob = 0;
    int det_prob = 0;
    int obj_x = 0, obj_y = 0, obj_w = 0, obj_h = 0;
    const gchar *model_name = "";
    
    if(NULL == handle) {
        return;
    }

    LogStruct* plogHandle = (LogStruct*)handle;
    std::ofstream& m_of = plogHandle->m_of;

    obj_x = meta->x;
    obj_y = meta->y;
    obj_w = meta->w;
    obj_h = meta->h;
    
    for (GList* l = meta->params; l; l = g_list_next(l)) {
        GstStructure * structure = (GstStructure *) l->data;
        if(gst_structure_has_name(structure, "object_id")) {
            //extract OT results; refer to vasobjecttracker code
            gst_structure_get_int(structure, "id", &object_id);
        }
        else if(gst_structure_has_name(structure, "detection")) {
            //extract detection results; refer to gvadetect code
            gst_structure_get_int(structure, "label_id", &det_label_id);
            double tmp_prob = 0.0;
            if(gst_structure_get_double(structure, "confidence", &tmp_prob))
                det_prob = (int)(tmp_prob*1000);
        }
        else if (gst_structure_has_field(structure, "label")) {
            // extract classify result; refer to gvaclassiy code
            gst_structure_get_int(structure, "label_id", &cls_label_id);
            double tmp_prob = 0.0;
            if(gst_structure_get_double(structure, "confidence", &tmp_prob))
                cls_prob = (int)(tmp_prob*1000);
            model_name = gst_structure_get_string(structure, "model_name");
        }
    }

    int frameStart = 0;
    int frameEnd = 0;
    int streamId = 0;
    int pts = 0;

    m_of << frameStart << ", " << frameEnd <<", 0, 0,";
    m_of << streamId << ", " << pts << ", ";
    m_of << obj_x << ", " << obj_y << ", "<< obj_w << ", "<< obj_h << ", ";
    m_of << object_id << ", " << det_label_id << ", " << det_prob <<", ";
    m_of << "1, " << model_name << ", "<< cls_label_id <<", "<< cls_prob <<std::endl;
    m_of.flush();
}

#endif
