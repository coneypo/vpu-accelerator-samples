# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT
#
noinst_PROGRAMS = unitsample
unitsample_SOURCES = test.cpp
unitsample_CXXFLAGS = $(GST_CFLAGS)  $(JSONC_CFLAGS)  -I $(top_builddir)/gst-libs/ -std=c++11  $(OPENCV_CFLAGS)
unitsample_LDADD = $(GST_LIBS) $(JSONC_LIBS) $(top_builddir)/gst-libs/libgapiobject.la -lstdc++ $(OPENCV_LIBS)
unitsample_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
