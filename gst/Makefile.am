# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT
#
plugin_LTLIBRARIES = libgstgapiosd.la
libgstgapiosd_la_SOURCES = gstgapiosd.cpp

libgstgapiosd_la_CFLAGS = $(GST_CFLAGS) $(JSONC_CFLAGS) -I $(top_builddir)/gst-libs/ -I $(top_srcdir)/gst-libs/
libgstgapiosd_la_CXXFLAGS = $(GST_CFLAGS)  $(JSONC_CFLAGS) -I $(top_builddir)/gst-libs/ -I $(top_srcdir)/gst-libs/ -std=c++11
libgstgapiosd_la_LIBADD = $(GST_LIBS) $(JSONC_LIBS) $(top_builddir)/gst-libs/libgapiobject.la
libgstgapiosd_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstgapiosd_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstgapiosd.h
