#!/bin/bash

# env variable to set: BUILD_DIR, INSTALL_DIR
# make sure you have obtained the access for 
#       dldt:           git@gitlab-icv.inn.intel.com:inference-engine/dldt.git
#       kmbplugin:      git@gitlab-icv.inn.intel.com:inference-engine/kmb-plugin.git
#       mcmCompiler:    git@github.com:movidius/mcmCompiler.git
#       gapi-sipp:      git@gitlab-icv.inn.intel.com:G-API/g-api-vpu.git

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
DOWNLOAD_DIR=${BUILD_DIR:="`pwd`/download/"}
HOST_INSTALL_DIR=${INSTALL_DIR:="`pwd`/host_install/dldt"}

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

if [ "$BUILD_DEBUG"x == 1x ]; then
    ENABLE_DEBUG=--enable-debug
    CMAKE_ENABLE_DEBUG=-DCMAKE_BUILD_TYPE=Debug
    BUILD_TYPE=Debug
else
    ENABLE_DEBUG=
    CMAKE_ENABLE_DEBUG=
    BUILD_TYPE=Release
fi

echo "BUILD_DEBUG       = ${BUILD_DEBUG}"
echo "BUILD_DIR         = ${BUILD_DIR}"
echo "INSTALL_DIR       = ${INSTALL_DIR}"

function download_dldt () {
    echo "Download dldt ..."
    cd ${DOWNLOAD_DIR}
    if [ ! -d ${DOWNLOAD_DIR}/dldt ]; then
        git clone git@gitlab-icv.inn.intel.com:inference-engine/dldt.git --branch master --single-branch ${DOWNLOAD_DIR}/dldt
    fi
    # cd dldt

    # cwd=$(pwd)

    # # clone Inference Engine submodules
    # git submodule init
    # git submodule update --recursive

    # # install Git LFS to clone IRs for regression tests
    # wget https://github.com/git-lfs/git-lfs/releases/download/v2.3.4/git-lfs-linux-amd64-2.3.4.tar.gz -P .
    # tar xf ~/git-lfs-linux-amd64-2.3.4.tar.gz -C .
    # cd ./git-lfs-2.3.4
    # sudo PREFIX=/usr/ ./install.sh
    # git config --global http.sslverify false
    # export https_proxy=http://proxy-chain.intel.com:911

    # # build Inference Engine
    # mkdir "$cwd/build"
    # cd "$cwd/build"
    # # set "-DENABLE_PRIVATE_MODELS=ON" to download private models from https://gitlab-icv.inn.intel.com/inference-engine-models/private-ir (if you have access)
    # cmake -DENABLE_TESTS=OFF -DVERBOSE_BUILD=ON -DENABLE_PERFORMANCE_TESTS=OFF -DENABLE_FUNCTIONAL_TESTS=OFF ${CMAKE_ENABLE_DEBUG} -DENABLE_PRIVATE_MODELS=OFF -DCMAKE_INSTALL_PREFIX=${HOST_INSTALL_DIR}/dldt ..
    # make -j8
    # make install
}

function download_and_build_hddl2plugin () {
    echo "Download hddl2plugin ..."
    cd ${DOWNLOAD_DIR}
    if [ ! -d ${DOWNLOAD_DIR}/kmb-plugin ]; then
        git clone git@gitlab-icv.inn.intel.com:inference-engine/kmb-plugin.git --branch master --single-branch ${DOWNLOAD_DIR}/kmb-plugin
    fi
    cd kmb-plugin

    # # Inform user about purposes of this script
    # echo "This script will make initial build of kmb-plugin after clonning from repository"
    # read -ep "Would You like to continiue? [Yes/No] " -i "Yes" isContiniue

    # if [ "$isContiniue" != "Yes" ]; then
    #     echo "Stop to build kmb-plugin"
    #     exit 0
    # else
    #     echo "Start to build kmb-plugin"
    # fi

    echo "=============== Directory search ==============="
    # Set base directories for DLDT and KMB-Plugin
    # Set path to KMB-Plugin project
    echo "Try to find base directories for dldt and kmb-plugin"
    export KMB_PLUGIN_HOME=$(pwd)
    echo "kmb-plugin base directory is: " $KMB_PLUGIN_HOME
    cd ..

    # Set path to DLDT project
    # Try to find DLDT in parent directory
    if [ -d "dldt" ]
    then
        cd dldt
        export DLDT_HOME=$(pwd)
    # If "dldt" directory is absent then ask user to set path to DLDT project
    else
        echo "Default path to DLDT project is not found"
        read -p "Input path to DLDT project: " DLDT_HOME
    fi
    echo "dldt base directory is: " $DLDT_HOME

    echo "=============== Install dependencies ==============="
    # Go to KMB-Plugin directory and instal some prerequisites for KMB-Plugin
    cd $KMB_PLUGIN_HOME
    # install Swig:
    sudo apt install -y swig
    # install python3-dev:
    sudo apt install -y python3-dev
    # install python-numpy:
    sudo apt install -y python-numpy
    # install metis:
    sudo apt install -y libmetis-dev libmetis5 metis

    # Begin to make DLDT for KMB-Plugin
    echo "=============== Build DLDT ==============="
    export BUILD_DIR_NAME=build

    echo "Begin to make DLDT for KMB-Plugin"
    cd $DLDT_HOME

    git checkout 86edc6163e7b02594316bcc151745c2ba7eb24a5
    git lfs pull
    git submodule init
    git submodule update --init --recursive
    mkdir -p $DLDT_HOME/$BUILD_DIR_NAME
    cd $DLDT_HOME/$BUILD_DIR_NAME
    # It is necessary to set -DENABLE_PLUGIN_RPATH=ON because in script in /dld/inference-engine/build-after-clone.sh this parameter is set to OFF
    # Path to libraries is necessary for properly work of test script (kmb-plugin/run_tests_after_build.sh)
    # cmake -DENABLE_TESTS=ON -DENABLE_BEH_TESTS=ON -DENABLE_FUNCTIONAL_TESTS=ON -DENABLE_PLUGIN_RPATH=ON -DENABLE_CLDNN=OFF -DENABLE_DLIA=OFF -DENABLE_MKL_DNN=OFF -DENABLE_GNA=OFF -DCMAKE_BUILD_TYPE=Debug ..
    cmake -DENABLE_TESTS=OFF -DVERBOSE_BUILD=ON -DENABLE_CLDNN=OFF -DENABLE_DLIA=OFF -DENABLE_MKL_DNN=OFF -DENABLE_GNA=OFF -DENABLE_PERFORMANCE_TESTS=OFF -DENABLE_PLUGIN_RPATH=ON -DENABLE_FUNCTIONAL_TESTS=OFF ${CMAKE_ENABLE_DEBUG} -DENABLE_PRIVATE_MODELS=OFF -DCMAKE_INSTALL_PREFIX=${HOST_INSTALL_DIR}/dldt ..

    make -j8
    make install

    # try to fix some bug with cmake in DLDT (it is necessary to delete targets_developer.cmake)
    rm $DLDT_HOME/$BUILD_DIR_NAME/targets_developer.cmake
    cd $DLDT_HOME/$BUILD_DIR_NAME/
    cmake ..

    echo "=============== Build KMB-Plugin ==============="
    # Begin to make KMB-Plugin
    echo "Begin to make KMB-Plugin"
    cd $KMB_PLUGIN_HOME
    export MCM_HOME=$KMB_PLUGIN_HOME/thirdparty/movidius/mcmCompiler

    git checkout 8b1ebe0804476343d93ca8415a6813ee2c9df661 
    git lfs pull
    git submodule update --init --recursive

    mkdir -p $KMB_PLUGIN_HOME/$BUILD_DIR_NAME
    cd $KMB_PLUGIN_HOME/$BUILD_DIR_NAME
    cmake -DInferenceEngineDeveloperPackage_DIR=$DLDT_HOME/build ${CMAKE_ENABLE_DEBUG} -DCMAKE_INSTALL_PREFIX=${HOST_INSTALL_DIR}/dldt ..
    make -j8
    make install

    cp -r $DLDT_HOME/inference-engine/temp/opencv_4.3.0_ubuntu18/opencv ${HOST_INSTALL_DIR}/dldt
    cp -r ${HOST_INSTALL_DIR}/dldt/lib/* ${HOST_INSTALL_DIR}/dldt/deployment_tools/inference_engine/lib/intel64/

    echo "Work of script is finished. Check logs for errors."
     
}


# install dependencies
sudo apt-get install -y build-essential \
        cmake \
        curl \
        wget \
        libssl-dev \
        ca-certificates \
        git \
        libboost-regex-dev \
        gcc-multilib \
        g++-multilib \
        libgtk2.0-dev \
        pkg-config \
        libcairo2-dev \
        libpango1.0-dev \
        libglib2.0-dev \
        libgtk2.0-dev \
        libswscale-dev \
        libavcodec-dev \
        libavformat-dev \
        libusb-1.0-0-dev


download_dldt
download_and_build_hddl2plugin

echo "dldt and hddl2plugin building is done!"
echo ""
