# kmb_IA_sample

This repo provides sample code for bypass and streaming mode, the sample is running on IA side and depends on hddlunite to offload DL workload into KMB.

# Build

## install openvino && build opencv
<https://gitlab.devtools.intel.com/kmb_hddl/samples/blob/master/README.md>

## build mfx plugin
<https://github.com/intel/gstreamer-media-SDK/tree/topic_linux_and_window>

## build GVA plugin
<https://github.com/opencv/gst-video-analytics> 

## build RemoteOffload plugin
<https://gitlab.devtools.intel.com/OWR/IoTG/GMS/Yocto/Graphics/Media/hddl_streamer/blob/streaming_mode_poc/README.md>


## build customize plugin and app 

```sh
# install opencl hearders
apt-get install opencl-c-headers

# set environment variable
export APP_HOME=<YOUR APPLICATION PROJECT ROOT PATH>

export GVA_HOME=<YOUR GVA PLUGIN ROOT PATH>

export GST_REMOTEOFFLOAD_PLUGIN_HOME=<YOUR REMOTEOFFLOADBIN PLUGIN ROOT PATH>


# configurate driver for gstmfx and vaappi
export LIBVA_DRIVER_NAME=iHD
export GST_VAAPI_ALL_DRIVERS=1
export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libxcb-dri3.so

export LD_LIBRARY_PATH=/usr/local/lib:${GVA_HOME}/build/intel64/Release/lib:${LD_LIBRARY_PATH}

# Set media SDK PATH
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/opt/intel/mediasdk/lib64/pkgconfig


mkdir build
cd build
cmake ..

make
cd app
```

*If meet issue:"Could not find a configuration file for package OpenCV that exactly matches requested version 4.0.0. please set CMAKE_PREFIX_PATH=<YOUR CUSTOM COMPILED OPENCV-4.0.0 INSTALL DIR>*

## set gst_plugin_path
```
export GST_REMOTEOFFLOAD_PLUGIN_PATH=${GST_REMOTEOFFLOAD_PLUGIN_HOME}/build/remoteoffloadext
export GST_PLUGIN_PATH=${APP_HOME}/build/output/gstreamer-1.0:${GST_REMOTEOFFLOAD_PLUGIN_HOME}/build/gstreamer-1.0:${GVA_HOME}/build/intel64/Release/lib:${GST_PLUGIN_PATH}
```
where the *.so in the corresponding path.

## validate
`
gst-inspect-1.0 vaapi
gst-inspect-1.0 mfx
gst-inspect-1.0 remoteoffload

gst-launch-1.0 filesrc location=<video path>/video.mp4 ! qtdemux  ! h264parse ! mfxh264dec ! inference ! osdparser ! mfxsink
`

## Run App in build dir


``` 
cd build/output
./hddldemo -c ../../app/config.txt 
```

## Run App in install dir

```
vim <install path>/config.txt to spcify video source
export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libxcb-dri3.so
export LD_LIBRARY_PATH='<install path>/lib'
export GST_PLUGIN_PATH='<install path>/lib'
cd <install path>/bin
./hddldemo -c ../config.json
```


## Config in bypass mode
make sure there are hva related configuration in config.json, you can refer to config_hva.json to setup your configuration. Remember to set syncmode=index for inference plugin in pipeline

## Config in streaming  mode
make sure there are no hva related configuration in config.json, you can refer to config.json to setup your configuration. Remember to set syncmode=pts for inference plugin in pipeline



### Knowning issue

Q. Build opencv error with protobuf
A. cmake -DBUILD_LIST=core,calib3d,imgproc,imgcodecs,highgui -DBUILD_PROTOBUF=OFF -DWITH_VA_INTEL=ON -DWITH_IPP=OFF -DWITH_CUDA=OFF -DOPENCV_GENERATE_PKGCONFIG=ON -DBUILD_TESTS=OFF
