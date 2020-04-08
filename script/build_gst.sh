#!/bin/bash

# env variable to set: BUILD_DIR, INSTALL_DIR

sudo apt-get install bison

SCRIPT_DIR = "$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
MY_ROOT_DIR=`pwd`
DOWNLOAD_DIR=${BUILD_DIR:="`pwd`/download/"}
HOST_INSTALL_DIR=${INSTALL_DIR:="`pwd`/host_install/gst"}

GST_VERSION=1.16.0

set -e
set -x

function show_help(){
    echo "============ USAGE ============== "
    echo $0 " [-h] [-d value]"
    echo "     -h: help"
    echo "     -d, --debug : value can be 0(default),1, switch to the debug version, will trigger rebuild (overwrite -i)"
    exit 0
}

# so host computer can edit it
#chown -R ${HOST_USER}:${HOST_USER} ${WORK_DIR}/_build/
#chown -R ${HOST_USER}:${HOST_USER} ${WORK_DIR}/_install/

BUILD_DEBUG=0
# https://stackoverflow.com/questions/192249/how-do-i-parse-command-line-arguments-in-bash
POSITIONAL=()
while [[ $# -gt 0 ]]
do
    key="$1"
    case $key in
        -d|--debug)
        BUILD_DEBUG=$2
        shift # past key
        shift # past argument
        ;;
        -h|--help)
        show_help
        exit 0
        ;;
        *)    # unknown option
        POSITIONAL+=("$1") # save it in an array for later
        shift # past argument
        ;;
    esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

if [ $# == 0 ]; then
    show_help
    exit 0
fi

if [ "$BUILD_DEBUG"x == 1x ]; then
    ENABLE_DEBUG=--enable-debug
    VAAPI_ENABLE_DEBUG=-DDEBUG=ON
    CMAKE_ENABLE_DEBUG=-DCMAKE_BUILD_TYPE=Debug
    BUILD_TYPE=Debug
else
    ENABLE_DEBUG=
    VAAPI_ENABLE_DEBUG=
    CMAKE_ENABLE_DEBUG=
    BUILD_TYPE=Release
fi

echo "BUILD_DEBUG       = ${BUILD_DEBUG}"
echo "BUILD_DIR         = ${BUILD_DIR}"
echo "INSTALL_DIR       = ${INSTALL_DIR}"

export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${HOST_INSTALL_DIR}/lib:${HOST_INSTALL_DIR}/lib/gstreamer-1.0
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:${HOST_INSTALL_DIR}/lib/pkgconfig

function create_folder () {
   if [ ! -d $1 ]; then
       mkdir $1
   fi
}

#download and build libva
function download_and_build_gst () {
    GST_DIR=${1}-${GST_VERSION}
    GST_SRC=${GST_DIR}.tar.xz
    echo "Download $GST_SRC ..."
    cd ${DOWNLOAD_DIR}
    if [ ! -f ${DOWNLOAD_DIR}/${GST_SRC} ]; then
        wget https://gstreamer.freedesktop.org/src/$1/${GST_SRC} -P ${DOWNLOAD_DIR}
    fi
    if [ ! -d ${DOWNLOAD_DIR}/${GST_DIR} ]; then
        tar -xvf ${GST_SRC}
        cd ${DOWNLOAD_DIR}/${GST_DIR}
        if [ $1 == "gst-plugins-bad" ]; then
            patch -p1 < ${SCRIPT_DIR}/0001-h265parse-Fix-for-st_rps_bits-calculation.patch
        fi
        ./autogen.sh --prefix=${HOST_INSTALL_DIR} --disable-gtk-doc --disable-oss4 $ENABLE_DEBUG
    fi
    cd ${DOWNLOAD_DIR}/${GST_DIR}
    make -j8
    make install
}

# function build_vaapi_shim_plugin () {
#     MY_HOME_DIR=${DOWNLOAD_DIR}/vaapi_bypass
#     if [ ! -d ${MY_HOME_DIR} ]; then
#         git clone ssh://git@gitlab.devtools.intel.com:29418/OWR/IoTG/GMS/Yocto/Graphics/Media/vaapi_bypass.git --branch multisession --single-branch ${MY_HOME_DIR}
#         cd ${MY_HOME_DIR}
#         patch -p1 < ../../0001-Fixing-issue-on-bypass-plugin.patch
#     fi
#     # gst plugin: libgstbypass.so
#     cd  ${MY_HOME_DIR}/sample/unite/
#     create_folder build
#     cd build
#     cmake .. && make
#     cp libgstbypass.so ${HOST_INSTALL_DIR}/lib
# }

function build_gst_vaapi_plugin () {
    MY_HOME_DIR=${DOWNLOAD_DIR}/gstreamer-vaapi
    if [ ! -d ${MY_HOME_DIR} ]; then
        git clone https://gitlab.devtools.intel.com/OWR/IoTG/GMS/Yocto/Graphics/Media/gstreamer-vaapi.git --branch master --single-branch ${MY_HOME_DIR}
        cd ${MY_HOME_DIR}
        git checkout ebea356cffed97a37aa98bb74e9c2a5d1fa7734
        patch -p1 < ${SCRIPT_DIR}/gstreamer-vaapi-fd_dup.patch
        ./autogen.sh --prefix=${HOST_INSTALL_DIR} --disable-gtk-doc --disable-oss4 $ENABLE_DEBUG
    fi
    cd ${MY_HOME_DIR}
    make -j12 && make install
}

download_and_build_gst gstreamer
download_and_build_gst gst-plugins-base
download_and_build_gst gst-plugins-good
download_and_build_gst gst-plugins-ugly
download_and_build_gst gst-plugins-bad
## download_and_build_gst gstreamer-vaapi
build_gst_vaapi_plugin
# build_vaapi_shim_plugin

cd ${MY_ROOT_DIR}

echo "GST building is done!"
echo ""
