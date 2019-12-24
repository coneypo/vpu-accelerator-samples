# kmb_IA_sample

This repo provides sample code for bypass and streaming mode, the sample is running on IA side and depends on hddlunite to offload DL workload into KMB.

# Build

## install openvino && build opencv
<https://gitlab.devtools.intel.com/kmb_hddl/samples/blob/master/README.md>

## build mfx plugin
<https://github.com/intel/gstreamer-media-SDK/tree/topic_linux_and_window>

## build customize plugin and app 

```sh
# Set media SDK PATH
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/opt/intel/mediasdk/lib64/pkgconfig

mkdir build
cd build
cmake ..
make
cd app
```
*osdparser is a gstreamer plugin for parse bounding box and test into GST overlay composition for further OSD processing*

## set gst_plugin_path
```
export GST_PLUGIN_PATH='...'
```
where the *.so in the corresponding path.

## validate
`
gst-launch-1.0 filesrc location=<video path>/video.mp4 ! qtdemux  ! h264parse ! mfxh264dec ! inference ! osdparser ! mfxsink
`

## Run App


``` 
cd build/output
./hddldemo -l ../../app/config.txt -r 1 -c 1
```
