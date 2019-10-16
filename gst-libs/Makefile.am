# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT
#
noinst_LTLIBRARIES = libgapiobject.la

libgapiobject_la_SOURCES = gapiobject.cpp gapiobjecttext.cpp jsonHandle.cpp gapiobjectrectangle.cpp gapiobjectline.cpp gapiobjectcircle.cpp

libgapiobject_la_CFLAGS = $(GST_CFLAGS)  $(JSONC_CFLAGS)
libgapiobject_la_CXXFLAGS = $(GST_CFLAGS)  $(JSONC_CFLAGS) $(OPENCV_CFLAGS) -std=c++11
libgapiobject_la_LIBADD = $(GST_LIBS) $(JSONC_LIBS) -lstdc++  $(OPENCV_LIBS)
libgapiobject_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

noinst_HEADERS = gapiobject.h gapiobjecttext.h jsonHandle.h common.h gapiobjectrectangle.h gapiobjectline.h gapiobjectcircle.h
