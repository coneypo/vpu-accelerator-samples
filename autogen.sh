#!/bin/sh
#
# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT
#
# you can either set the environment variables AUTOCONF, AUTOHEADER, AUTOMAKE,
# ACLOCAL, AUTOPOINT and/or LIBTOOLIZE to the right versions, or leave them
# unset and get the defaults

autoreconf --verbose --force --install --make || {
 echo 'autogen.sh failed';
 exit 1;
}

echo
echo "Now type './configure --with-opencv-path=/opt/openvino/opencv/' to create the Makefile."
echo "Then type 'make ' to finish compile."
echo
