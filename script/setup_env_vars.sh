#!/bin/bash

export BUILD_DIR=/home/cpival/zhicheng/kezhen/build
export INSTALL_DIR=/home/cpival/zhicheng/kezhen/install 
export INSTALL_DIR_KMB=/home/cpival/zhicheng/kezhen/installkmb
export HDDLUNITE_HOST_INSTALL_DIR=/home/cpival/zhicheng/kezhen/hddlunite_install
export HDDLUNITE_KMB_INSTALL_DIR=/home/cpival/zhicheng/kezhen/hddlunite_installkmb
export HDDLUNITE_ROOT_DIR=/home/cpival/zhicheng/kezhen/hddlunite

export LD_LIBRARY_PATH=${HDDLUNITE_KMB_INSTALL_DIR}/lib:${INSTALL_DIR}/lib:${INSTALL_DIR}/lib/gstreamer-1.0:${LD_LIBRARY_PATH}
export PKG_CONFIG_PATH=${INSTALL_DIR}/lib/pkgconfig:$PKG_CONFIG_PATH
export GST_PLUGIN_PATH=${INSTALL_DIR}/lib:${INSTALL_DIR}/lib/gstreamer-1.0
export PATH=$INSTALL_DIR/bin/:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin
export LIBRARY_PATH=$INSTALL_DIR/lib:$INSTALL_DIR/lib/gstreamer-1.0