dnl Copyright (C) 2019 Intel Corporation
dnl SPDX-License-Identifier: MIT
dnl

dnl required version of autoconf
AC_PREREQ([2.53])

dnl TODO: fill in your package name and package version here
AC_INIT([my-plugin-package],[1.0.0])
AC_PROG_CXX([gcc cl KCC CC cxx cc++ xlC aCC c++ g++])

dnl required versions of gstreamer and plugins-base
GST_REQUIRED=1.0.0
GSTPB_REQUIRED=1.0.0

AC_CONFIG_SRCDIR([gst/gstgapiosd.cpp])
AC_CONFIG_HEADERS([config.h])

dnl required version of automake
AM_INIT_AUTOMAKE([1.10])

dnl enable mainainer mode by default
AM_MAINTAINER_MODE([enable])

dnl check for tools (compiler etc.)
AC_PROG_CC


dnl required version of libtool
LT_PREREQ([2.2.6])
LT_INIT

dnl give error and exit if we don't have pkgconfig
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, [ ], [
  AC_MSG_ERROR([You need to have pkg-config installed!])
])

dnl Check for the required version of GStreamer core (and gst-plugins-base)
dnl This will export GST_CFLAGS and GST_LIBS variables for use in Makefile.am
dnl
dnl If you need libraries from gst-plugins-base here, also add:
dnl for libgstaudio-1.0: gstreamer-audio-1.0 >= $GST_REQUIRED
dnl for libgstvideo-1.0: gstreamer-video-1.0 >= $GST_REQUIRED
dnl for libgsttag-1.0: gstreamer-tag-1.0 >= $GST_REQUIRED
dnl for libgstpbutils-1.0: gstreamer-pbutils-1.0 >= $GST_REQUIRED
dnl for libgstfft-1.0: gstreamer-fft-1.0 >= $GST_REQUIRED
dnl for libgstinterfaces-1.0: gstreamer-interfaces-1.0 >= $GST_REQUIRED
dnl for libgstrtp-1.0: gstreamer-rtp-1.0 >= $GST_REQUIRED
dnl for libgstrtsp-1.0: gstreamer-rtsp-1.0 >= $GST_REQUIRED
dnl etc.
PKG_CHECK_MODULES(GST, [
  gstreamer-1.0 >= $GST_REQUIRED
  gstreamer-base-1.0 >= $GST_REQUIRED
  gstreamer-controller-1.0 >= $GST_REQUIRED
  gstreamer-audio-1.0 >= $GST_REQUIRED
  gstreamer-video-1.0 >= $GST_REQUIRED
  gstreamer-check-1.0 >= $GST_REQUIRED
], [
  AC_SUBST(GST_CFLAGS)
  AC_SUBST(GST_LIBS)
], [
  AC_MSG_ERROR([
      You need to install or upgrade the GStreamer development
      packages on your system. On debian-based systems these are
      libgstreamer1.0-dev and libgstreamer-plugins-base1.0-dev.
      on RPM-based systems gstreamer1.0-devel, libgstreamer1.0-devel
      or similar. The minimum version required is $GST_REQUIRED.
  ])
])

PKG_CHECK_MODULES(JSONC, [
   json-c
], [
  AC_SUBST(JSONC_CFLAGS)
  AC_SUBST(JSONC_LIBS)
], [
  AC_MSG_ERROR([
      You need to install libjson-c ])
])

PKG_CHECK_MODULES(OPENCV, [
   opencv
], [
  AC_SUBST(OPENCV_CFLAGS)
  AC_SUBST(OPENCV_LIBS)
], [
  AC_MSG_WARN([
      can't find opencv by pkg-config, use /usr/local/ and /opt/opencv as default path.])
  AC_MSG_NOTICE([you can use ./configure --with-opencv-path=path option input opencv path.])
  AC_SUBST(OPENCV_CFLAGS,"-I/usr/local/include/opencv4 -I/opt/opencv/include")
  AC_SUBST(OPENCV_LIBS,"-L/usr/local/lib -L/opt/opencv/lib -lopencv_imgproc \
           -lopencv_gapi -lopencv_core -lopencv_video -lopencv_highgui -lopencv_imgcodecs")
])

dnl support input opencv path
AC_ARG_WITH([opencv-path],
            [AS_HELP_STRING([--with-opencv-path],
                            [support opencv path input@<:@default=null@:>@])],
                            [
                             AC_MSG_NOTICE([The opencv path you set is $with_opencv_path.])
                            ],
                            [with_opencv_path=null])

AS_IF([test "x$with_opencv_path" != xnull],
      [
       AC_SUBST(OPENCV_CFLAGS,"-I$with_opencv_path/include/")
       AC_SUBST(OPENCV_LIBS,"-L$with_opencv_path/lib/ -lopencv_imgproc \
                -lopencv_gapi -lopencv_core -lopencv_video -lopencv_highgui -lopencv_imgcodecs")
       ])
dnl check if compiler understands -Wall (if yes, add -Wall to GST_CFLAGS)
AC_MSG_CHECKING([to see if compiler understands -Wall])
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wall"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([ ], [ ])], [
  GST_CFLAGS="$GST_CFLAGS -Wall"
  AC_MSG_RESULT([yes])
], [
  AC_MSG_RESULT([no])
])
GST_CFLAGS="$GST_CFLAGS -Wformat -Wformat-security -Werror=format-security -fstack-protector-strong -fno-strict-overflow -fno-delete-null-pointer-checks -fwrapv -fPIE -pie -z noexecstack -z,now -z,relro -D_FORTIFY_SOURCE=2"

dnl set the plugindir where plugins should be installed (for src/Makefile.am)
if test "x${prefix}" = "x$HOME"; then
  plugindir="$HOME/.gstreamer-1.0/plugins"
else
  plugindir="\$(libdir)/gstreamer-1.0"
fi
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

AC_CONFIG_FILES([Makefile gst-libs/Makefile gst/Makefile sample/Makefile  unittest/Makefile])
AC_OUTPUT

