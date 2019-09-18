#!/bin/bash
#
# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT
#
gst-launch-1.0 videotestsrc ! video/x-raw,format=NV12,width=1920,height=1080 ! api2d config-path=/home/huzhaoyang/Working/gstApi2D/gst_api_2d/sample/osd_config.json  ! videoconvert ! ximagesink
