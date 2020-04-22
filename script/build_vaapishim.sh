#!/bin/bash

# env variable to set: BUILD_DIR, INSTALL_DIR, INSTALL_DIR_KMB
#                      HDDLUNITE_HOST_INSTALL_DIR, HDDLUNITE_KMB_INSTALL_DIR
#                      HDDLUNITE_ROOT_DIR

sudo apt-get -y install autoconf libdrm-dev cmake make libx11-dev

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
DOWNLOAD_DIR=${BUILD_DIR:="`pwd`/download/"}
HOST_INSTALL_DIR=${INSTALL_DIR:="`pwd`/host_install/vaapi_bypass"}
KMB_INSTALL_DIR=${INSTALL_DIR_KMB:="`pwd`/kmb_install/vaapi_bypass"}

HDDLUNITE_HOST_INSTALL_DIR=${HDDLUNITE_HOST_INSTALL_DIR:="`pwd`/host_install/hddlunite"}
HDDLUNITE_KMB_INSTALL_DIR=${HDDLUNITE_KMB_INSTALL_DIR:="`pwd`/kmb_install/hddlunite"}
HDDLUNITE_ROOT_DIR=${HDDLUNITE_ROOT_DIR:="`pwd`/.."}

set -e
set -x

function show_help(){
    echo "============ USAGE ============== "
    echo $0 " [-h] [-k value] [-d value]"
    echo "     -h: help"
    echo "     -k, --build_kmb : build kmb repos, value can be 0(default) or 1"
    echo "     -d, --debug : value can be 0(default),1, switch to the debug version, will trigger rebuild (overwrite -i)"
    exit 0
}

# so host computer can edit it
#chown -R ${HOST_USER}:${HOST_USER} ${WORK_DIR}/_build/
#chown -R ${HOST_USER}:${HOST_USER} ${WORK_DIR}/_install/

BUILD_DEBUG=0
BUILD_KMB=0
# https://stackoverflow.com/questions/192249/how-do-i-parse-command-line-arguments-in-bash
POSITIONAL=()
while [[ $# -gt 0 ]]
do
    key="$1"
    case $key in
        -k|--build_kmb)
        BUILD_KMB=$2
        shift # past key
        shift # past argument
        ;;
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
echo "BUILD_KMB         = ${BUILD_KMB}"
echo "BUILD_DIR         = ${BUILD_DIR}"
echo "INSTALL_DIR       = ${INSTALL_DIR}"
echo "BUILD_DIR_KMB     = ${BUILD_DIR_KMB}"
echo "INSTALL_DIR_KMB   = ${INSTALL_DIR_KMB}"
echo "--"
echo "DOWNLOAD_DIR      = ${DOWNLOAD_DIR}"
echo "HOST_INSTALL_DIR  = ${HOST_INSTALL_DIR}"
echo "KMB_INSTALL_DIR   = ${KMB_INSTALL_DIR}"

function create_folder () {
    if [ ! -d $1 ]; then
        mkdir -p $1
    fi
}

# download and build libva
function build_libva () {
    if [ ! -d ${DOWNLOAD_DIR}/libva ]; then
        git clone  https://github.com/intel/libva.git  --single-branch ${DOWNLOAD_DIR}/libva
        cd ${DOWNLOAD_DIR}/libva
        ./autogen.sh --with-drivers-path=/usr/lib/x86_64-linux-gnu/dri --prefix=$HOST_INSTALL_DIR $ENABLE_DEBUG
    fi
    cd ${DOWNLOAD_DIR}/libva
    ./autogen.sh --with-drivers-path=/usr/lib/x86_64-linux-gnu/dri --prefix=$HOST_INSTALL_DIR $ENABLE_DEBUG
    make -j8
    make install
}

# download and build safestringlib
function build_safestringlib () {
    if [ ! -d ${DOWNLOAD_DIR}/safestringlib ]; then
        git clone https://github.com/intel/safestringlib  --single-branch ${DOWNLOAD_DIR}/safestringlib
        cp -rf ${DOWNLOAD_DIR}/safestringlib ${DOWNLOAD_DIR}/safestringlib_arm
        cd ${DOWNLOAD_DIR}/safestringlib_arm
        patch -p1 < ${SCRIPT_DIR}/0001-fix-cc-build-error-mmitigate-rop.patch
    fi

    cd ${DOWNLOAD_DIR}/safestringlib
    create_folder build
    cd build
    cmake .. && make -j8

    if [ "$BUILD_KMB"x == 1x ]; then
        #cc safestringlib
        cd ${DOWNLOAD_DIR}/safestringlib_arm
        create_folder build
        cd build
        cmake .. -DCMAKE_TOOLCHAIN_FILE=${HDDLUNITE_ROOT_DIR}/cmake/aarch64.cmake
        make -j8
    fi
}

# download vaapi-bypass
function build_vaapi_shim () {
    MY_HOME_DIR=${DOWNLOAD_DIR}/vaapi_bypass
    if [ ! -d ${MY_HOME_DIR} ]; then
        git clone ssh://git@gitlab.devtools.intel.com:29418/OWR/IoTG/GMS/Yocto/Graphics/Media/vaapi_bypass.git --branch multisession --single-branch ${MY_HOME_DIR}
        cd ${MY_HOME_DIR}
        patch -p1 < ${SCRIPT_DIR}/0001-Fixing-issue-on-bypass-plugin.patch
    fi
    cd ${MY_HOME_DIR}
    create_folder build
    cd build
    export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:${HOST_INSTALL_DIR}/lib/pkgconfig
    export SAFESTR_HOME=${DOWNLOAD_DIR}/safestringlib
    export HDDLUNITE=1
    #vaapi shim set /opt/intel/hddlunite as default search path
    sudo mkdir -p /opt/intel/hddlunite
    sudo cp -r ${HDDLUNITE_HOST_INSTALL_DIR}/* /opt/intel/hddlunite/

    unset XLINK_HOME
    pwd
    find /opt/intel -name libHddlUnite.so 
    ls /opt/intel/hddlunite
    ls /opt/intel/hddlunite/bin
    ls /opt/intel/hddlunite/lib
    cmake .. -DTARGETS=IA $VAAPI_ENABLE_DEBUG -DCMAKE_INSTALL_PREFIX=${HOST_INSTALL_DIR}
    make -j4
    # cp hddl_bypass_drv_video.so ${HOST_INSTALL_DIR}/lib
    make install
    cp ../connection.cfg ${HOST_INSTALL_DIR}

    # gst plugin: libgstbypass.so
    cd ${MY_HOME_DIR}/sample/unite/
    create_folder build
    cd build
    cmake $CMAKE_ENABLE_DEBUG .. && make
    cp libgstbypass* ${HOST_INSTALL_DIR}/lib

    if [ "$BUILD_KMB"x == 1x ]; then
        # cc vaapi_bypass
        cd ${MY_HOME_DIR}
        create_folder build_arm
        cd build_arm
        export SAFESTR_HOME=${DOWNLOAD_DIR}/safestringlib_arm
        export HDDLUNITE=1
        # export XLINK_HOME=/usr/local/oecore-x86_64/sysroots/aarch64-ese-linux/usr/lib
        sudo mkdir -p /usr/local/oecore-x86_64/sysroots/aarch64-ese-linux/opt/intel/hddlunite/
        sudo cp -rf ${HDDLUNITE_KMB_INSTALL_DIR}/* /usr/local/oecore-x86_64/sysroots/aarch64-ese-linux/opt/intel/hddlunite/.
        export C_INCLUDE_PATH=${HDDLUNITE_ROOT_DIR}/device_client/include
        cmake .. -DTARGETS=KMB -DCMAKE_TOOLCHAIN_FILE=${HDDLUNITE_ROOT_DIR}/cmake/aarch64.cmake $VAAPI_ENABLE_DEBUG
        make -j4
        cp hddl_bypass_shim ${KMB_INSTALL_DIR}
        cp ../connection.cfg ${KMB_INSTALL_DIR}
    fi
}


create_folder ${DOWNLOAD_DIR}
create_folder ${HOST_INSTALL_DIR}
create_folder ${KMB_INSTALL_DIR}

build_libva
build_safestringlib
build_vaapi_shim

echo "Vaapishim build is done."
echo ""

