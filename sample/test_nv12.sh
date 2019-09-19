#!/bin/bash
#
# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT
#
gst-launch-1.0 videotestsrc ! video/x-raw,format=NV12,width=1920,height=1080 ! gapiosd config-path=./osd_config.json  ! videoconvert ! ximagesink
