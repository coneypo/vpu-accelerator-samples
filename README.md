# kmb_IA_sample

This repo provides sample code for bypass and streaming mode, the sample is running on IA side and depends on hddlunite to offload DL workload into KMB.

# Build

## Install openvino with opencv intergrated
Please refer installation guide on <https://docs.openvinotoolkit.org/latest/_docs_install_guides_installing_openvino_linux.html>


## [Optional] Build RemoteOffload plugin
This plugin is only used for hddl streaming mode. You can skip it if running on bypass mode
<https://gitlab.devtools.intel.com/OWR/IoTG/GMS/Yocto/Graphics/Media/hddl_streamer/blob/streaming_mode_poc/README.md>


## [Optional] Build GVA plugin
This plugin is only used for hddl streaming mode. You can skip it if runing on bypass mode
<https://github.com/opencv/gst-video-analytics>


## [Optional] Install opencv with intel VA support
1. Install opencl dependencies
```sh
apt-get install opencl-c-headers
```

2.Please refer opencv installation section on <https://gitlab.devtools.intel.com/kmb_hddl/samples/blob/master/README.md>

## Build application

```sh
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/opt/intel/mediasdk/lib64/pkgconfig
mkdir build
cd build
cmake ..
make -j
```

If you have installed opencv with intel VA support, please set cmake parameter as following, then make it again
```sh
cmake -DENABLE_INTEL_VA_OPENCL=ON  -DINTEL_VA_ENABLED_OPENCV_DIR=<Opencv Install DIR>/cmake/opencv4 ..
make -j
```

If you need to build application for streaming mode. please set -DENABLE_HVA=OFF. 


## Setup running environment

1.setup application running environment 
```sh
export APP_HOME=<your application root path>
export LIBVA_DRIVER_NAME=iHD
export GST_VAAPI_ALL_DRIVERS=1
export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libxcb-dri3.so
export GST_PLUGIN_PATH=${APP_HOME}/build/output/gstreamer-1.0:${GST_PLUGIN_PATH}
```

2.[optional] setup GVA/RemoteOffload plugin path
```sh
export GVA_HOME=<your gva root path>  
export GST_REMOTEOFFLOAD_PLUGIN_HOME=<your remoteoffloadbin root path>
export LD_LIBRARY_PATH=${GVA_HOME}/build/intel64/Release/lib:${LD_LIBRARY_PATH}
export GST_REMOTEOFFLOAD_PLUGIN_PATH=${GST_REMOTEOFFLOAD_PLUGIN_HOME}/build/remoteoffloadext
export GST_PLUGIN_PATH=${GST_REMOTEOFFLOAD_PLUGIN_HOME}/build/gstreamer-1.0:${GVA_HOME}/build/intel64/Release/lib:${GST_PLUGIN_PATH}
```

3.validate application gst plugin path
```sh
gst-inspect-1.0 mfx
gst-inspect-1.0 mfxh264dec
gst-inspect-1.0 inference
gst-inspect-1.0 osdparser
gst-launch-1.0 filesrc location=<your h264 media file path> ! h264parse ! mfxh264dec ! osdparser ! mfxsink
```

4.[optional] validate gst plugin path for streaming mode
```sh
gst-inspect-1.0 vaapi
gst-inspect-1.0 vaapih264dec
gst-inspect-1.0 remoteoffload
gst-inspect-1.0 gvadetect
```

## Run

* Running bypass mode
``` 
#generate config file for application on bypass mode
cd ${APP_HOME}/app
./config_generator.sh -m bypass -n 1
mv config_for_hva_sample.json <Hva working directory>

# run application
cd ${APP_HOME}/build/output
./hddldemo -c ../../app/config_generated.json
```

* Running streaming mode
``` 
#generate config file for application on bypass mode
cd ${APP_HOME}/app
./config_generator.sh -m streaming -n 1

# run application
cd ${APP_HOME}/build/output
./hddldemo -c ../../app/config_generated.json
```


## Configuration Details
* bypass mode

Hva related configuration are required in config file. You can refer to config_hva.json to setup your configuration.

Remember to set <b>syncmode=index</b> for inference plugin in pipeline

* streaming mode

Hva related configuration must be removed from config file. You can refer to config.json to setup your configuration. 


Remember to set <b>syncmode=pts</b> for inference plugin in pipeline

## Known issue

<b>Q: protobuf related error when build opencv</b>

A: You can simple remove protobuf support.
```sh
 cmake -DBUILD_LIST=core,calib3d,imgproc,imgcodecs,highgui -DBUILD_PROTOBUF=OFF -DWITH_VA_INTEL=ON -DWITH_IPP=OFF -DWITH_CUDA=OFF -DOPENCV_GENERATE_PKGCONFIG=ON -DBUILD_TESTS=OFF
```
