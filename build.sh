# install requirements
sudo apt update
sudo apt install -y gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly gstreamer1.0-plugins-bad libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
sudo apt install -y libudev-dev qtbase5-dev libboost-program-options-dev libboost-thread-dev libboost-filesystem-dev opencl-c-headers libgstreamer-plugins-bad1.0-dev libxkbcommon-dev libxrandr-dev
sudo apt install -y git cmake

# clean build environment
BuildDIR="auto_build"
SourceDIR=$(pwd)
rm ./${BuildDIR} -rf
cd ${SourceDIR}
mkdir ${BuildDIR}

# install OpenVINO
cd ${SourceDIR}
OpenVINOPKG=$(find . -maxdepth 1 -name "*openvino_toolkit*" -type f -printf "%f\n")
if test -z "$OpenVINOPKG";
then
    echo "OpenVINO package was not found"
    echo "Please download from https://software.intel.com/en-us/openvino-toolkit/choose-download/free-download-linux"
    echo "And copy zipped file to current folder, otherwise it will assume that the package has been installed in advance."
    sleep 5
else
    echo "find OpenVINO package: ${OpenVINOPKG}, installing this to system....."
    tar -xzvf ./${OpenVINOPKG} -C ./${BuildDIR}
    cd ./${BuildDIR}/*openvino_toolkit*
    sudo -E ./install_openvino_dependencies.sh
    sed -i 's/ACCEPT_EULA=decline/ACCEPT_EULA=accept/' silent.cfg
    sudo ./install_GUI.sh -s silent.cfg
    source /opt/intel/openvino/bin/setupvars.sh
    cd /opt/intel/openvino/install_dependencies
    sudo -E ./install_NEO_OCL_driver.sh
    sudo usermod -a -G video $(whoami)
fi

# install OpenCL SDK
cd ${SourceDIR}
OpenCLSDK=$(find . -maxdepth 1 -name "*intel_sdk_for_opencl*" -type f -printf "%f\n")
if test -z "$OpenCLSDK";
then
    echo "OpenCL SDK package was not found"
    echo "Please download from https://software.intel.com/en-us/opencl-sdk"
    echo "And copy zipped file to current folder, otherwise it will assume that the package has been installed in advance."
    sleep 5
else
    echo "find OpenCL SDK package: ${OpenCLSDK}, installing this to system....."
    tar -xzvf ./${OpenCLSDK} -C ./${BuildDIR}
    cd ./${BuildDIR}/*intel_sdk_for_opencl*
    sed -i 's/ACCEPT_EULA=decline/ACCEPT_EULA=accept/' silent.cfg
    sudo -E ./install.sh -s silent.cfg
fi

## build & install OpenCV
cd ${SourceDIR}/${BuildDIR}
git clone --depth 1  https://github.com/opencv/opencv.git --branch 4.0.0-rc --single-branch
cd opencv
mkdir ${BuildDIR}
cd ${BuildDIR}
cmake -DWITH_VA_INTEL=ON -DWITH_IPP=OFF -DWITH_CUDA=OFF -DOPENCV_GENERATE_PKGCONFIG=ON -DBUILD_TESTS=OFF ..
sudo ln -sf /opt/intel/mediasdk/lib64/libva.so /usr/lib/libva.so
sudo ln -sf /opt/intel/mediasdk/lib64/libva-drm.so /usr/lib/libva-drm.so
make -j8
sudo make install

## build this repo
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/opt/intel/mediasdk/lib64/pkgconfig
cd ${SourceDIR}/${BuildDIR}
cmake ..
make -j8
