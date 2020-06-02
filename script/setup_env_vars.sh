#!/bin/bash

export BUILD_DIR=/mnt/mydisk1/my_project/ByPassModel/build
export INSTALL_DIR=/mnt/mydisk1/my_project/ByPassModel/install
export INSTALL_DIR_KMB=/mnt/mydisk1/my_project/ByPassModel/INSTALL_DIR_KMB
export HDDLUNITE_HOST_INSTALL_DIR=/mnt/mydisk1/my_project/ByPassModel/host_install_directory
export HDDLUNITE_KMB_INSTALL_DIR=/mnt/mydisk1/my_project/ByPassModel/arm_install_directory
export HDDLUNITE_ROOT_DIR=/mnt/mydisk1/my_project/ByPassModel/hddlunite

export LD_LIBRARY_PATH=${HDDLUNITE_HOST_INSTALL_DIR}/lib:${INSTALL_DIR}/lib:${INSTALL_DIR}/lib/gstreamer-1.0:${LD_LIBRARY_PATH}
export PKG_CONFIG_PATH=${INSTALL_DIR}/lib/pkgconfig:$PKG_CONFIG_PATH
export GST_PLUGIN_PATH=${INSTALL_DIR}/lib:${INSTALL_DIR}/lib/gstreamer-1.0
export PATH=$INSTALL_DIR/bin/:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin

export LIBVA_DRIVERS_PATH=/mnt/mydisk1/my_project/ByPassModel/install/lib:/opt/intel/mediasdk/lib64
export GST_PLUGIN_PATH=/mnt/mydisk1/my_project/ByPassModel/install/lib:/mnt/mydisk1/my_project/ByPassModel/install/lib/gstreamer-1.0
export CONFIG_PATH=/mnt/mydisk1/my_project/ByPassModel/build/vaapi_bypass/connection.cfg
