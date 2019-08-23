#!/bin/bash

gst-launch-1.0 videotestsrc ! video/x-raw,format=BGR,width=1920,height=1080 ! api2d config-path=/home/huzhaoyang/Working/gstApi2D/gstapi2d/sample/osd_config.json  ! videoconvert ! ximagesink
