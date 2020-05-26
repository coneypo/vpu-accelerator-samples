# KMB sample (gstream decoding + HVA detection/classifcation)

A sample Bypass Pipeline

This sample runs on IA host, with decoding offloaded to KMB and IE (detection + classification) on VPU. This sample now supports multistream, where at most we have tested 4 streams simultaneously at the moment.

# Update

- Async IE API is used to run inference, classification runs in sync mode because of requirement of object selection.
- NN model updated from u8 to fp16 model, please run with fp16 models compiled with IR in openvino package(`resnet50_uint8_int8_weights_pertensor.xml`, `tiny_yolo_v2_uint8_int8_weights_pertensor.xml`).
- Short term object tracking added. Classification will only run for new objects, previous classification results will be used for tracked objects.
- For frames droped by FRC, detection will be skipped and ROI will be given by short-term object tracking.
- Object tracking relies on OpenCV 4.3, if there is link error with OpenCV please update OpenCV to version 4.3.
- Known bug: 4 stream runs OK, but 6 stream fails to start.

#### External dependencies

- [Gstreamer (for decoding)](#gstreamer)
- [Boost](#boost)
- [VAAPI Shim](#vaapi-shim)
- [OpenVino](#openvino-dependency)
- [Kmb IA GUI Application](#kmb-ia-gui-application)

The sections that each entity points briefly introduces the requirements and version of each component. To facilitate users to set up the envrironment, some scripts are provided under `script/`. Below are the steps to set up the environment using the scripts provided.

1. build and install hddlunite. Hddlunite also provides a script of installation and as a component this sample replies on, it can be installed to anywhere  with freedom of choice
2. if you would like to use the dependency install script, make sure you have the access for
    1. vaapi shim repo gitlab.devtools.intel.com:29418/OWR/IoTG/GMS/Yocto/Graphics/Media/vaapi_bypass.git
    2. gstreamer vaapi plugin iotg build, https://gitlab.devtools.intel.com/OWR/IoTG/GMS/Yocto/Graphics/Media/gstreamer-vaapi.git
    3. dldt repo, gitlab-icv.inn.intel.com:inference-engine/dldt.git
    4. kmbplugin repo, gitlab-icv.inn.intel.com:inference-engine/kmb-plugin.git
    5. mcmCompiler repo, github.com:movidius/mcmCompiler.git
    6. gapi-sipp, gitlab-icv.inn.intel.com:G-API/g-api-vpu.git
3. set environment variables in `script/setup_env_vars.sh`
    1. `BUILD_DIR` refers to where those depended repos will be cloned/downloaded
    2. `INSTALL_DIR` refers to where the libraries and binaries will be installed
    3. `INSTALL_DIR_KMB` refers to where the libraries and binaries for KMB will be installed
    4. `HDDLUNITE_HOST_INSTALL_DIR` should be set to where hddlunite is installed (not the repo location but the installed location) in step 1
    5. `HDDLUNITE_KMB_INSTALL_DIR` should be set to where hddlunite for KMB ARM is installed
    6. `HDDLUNITE_ROOT_DIR` should be set to the hddlunite repo directory
4. `source script/setup_env_vars.sh`
5. `script/build_gst.sh -d 0` to build and install gstreamers
6. `script/build_vaapishim.sh -d 0 -k 0` where -d 0 refers to release build and -d 1 refers to debug build. -k 0 refers to to NOT build KMB binaries and libraries, to build and install vaapi shim
7. `script/build_dldt.sh -d 0` to build and install dldt and hddl2plugins

#### Build

If you have already had all the above dependencies ready, to compile this sample, just simply do:

```shell
git clone https://gitlab.devtools.intel.com/kmb_integration/gsthvasample.git
cd gsthvasample
git checkout feature/for_scan
```

Then before carrying on the compilation, make sure you have the following environment variables configured properly, especially when you install all those dependencies to a custom location rather than the system root, i.e. `/usr/`.

```shell
export HDDLUNITE_HOST_INSTALL_DIR=...

export GST_PLUGIN_PATH=...
export LD_LIBRARY_PATH=...
export PKG_CONFIG_PATH=...
export PATH=...
export LIBRARY_PATH=...
```

where `HDDLUNITE_HOST_INSTALL_DIR` should point to the directory where hddlunite is installed, e.g. `/opt/intel/hddlunite`.

Those variables including `GST_PLUGIN_PATH`, `LD_LIBRARY_PATH`, `PKG_CONFIG_PATH`, `PATH`, `LIBRARY_PATH` are required if you reroute components' install path to a different path.

Then do:

```shell
mkdir build
cd build
cmake ..
make
```

#### Configuration

Users can configure HVA pipeline through a config file, which will be searched by the application under current working directory upon boot up. The config file is divided into a few components, including GUI, Decoder, Detection, Classification and FRC etc. Below explains the meaning of each field.

- Detection / Classification
  - Model: the location of model file used by pipeline(models are blobs from EVM BKC path `/opt/`, detection model is "yolotiny.blob", classification model is "resnet.blob")
- Decode
  - Video: the video file fed into pipeline
  - DropXFrame: Drop `DropXFrame` frames every `DropEveryXFrame` frames by decoder
  - DropEveryXFrame: Drop `DropXFrame` frames every `DropEveryXFrame` frames by decoder
  - Codec: the codec of video file. Now this sample supports `h264`, `h265` and `mp4` format. Note that for h265 video, decoded frames are of `P010` (10-bit planar YUV format) rather `NV12` format, which isn't supported by current IE and jpeg encoder, which means IE would be highly likely to not detect anything and encoded jpegs are corrupted.
- GUI
  - Socket: The socket that HVA opens and listens for messages from GUI application
- FRC
  - DropXFrame: Drop `DropXFrame` frames every `DropEveryXFrame` frames by Frame Rate Control Node
  - DropEveryXFrame: Drop `DropXFrame` frames every `DropEveryXFrame` frames by Frame Rate Control Node

#### Run

##### if testing with Fake GUI Testcase

1. Prepare the same envrionemnt as Hddlunite and vaapi shim required and navigate to the directory where the config.json is located
2. Start HVA pipeline, e.g.
`./build/src/FullPipe/FullPipe`
3. Wait until the application prints out "Set socket to listening"
4. Start the fake GUI testcase, e.g.
`./build/src/FullPipeGUITest/FullPipeGUITest`
5. The fake GUI testcase will print out the detected ROI infos and labels etc.

##### if testing with Fake GUI Testcase with 2-stream use case

1. Prepare the same envrionemnt as Hddlunite and vaapi shim required and navigate to the directory where the config.json is located
2. Start HVA pipeline, e.g.
`./build/src/FullPipe/FullPipe`
3. Wait until the application prints out "Set socket to listening"
4. Start an idle Socket listenser without signaling HVA pipeline like the following, note the argument `1` at the end:
`./build/src/FullPipeGUITestSock/FullPipeGUITestSock 1`
5. Start the fake GUI testcase like the following, note the argument `1` at the end.
`./build/src/FullPipeGUITest/FullPipeGUITest 1`
6. This will sets up two sockets listens for detected ROI messages and the respective fake GUI testcase will print out the detected ROI infos and labels etc.

##### if testing with GUI application

The GUI application will start HVA pipeline automatically. In order to make the GUI locate and boot HVA pipeline, a few configurations and environment variable should be set in GUI application's config file. An example GUI config file is provided under `external/`

#### Known Issue

- Vaapi and IE may experience stability issue after ~10000 frames upon the first execution of a clean boot. Upon such failures the application should quit with appropriate printout on terminal
- Xlink Hang issue. Xlink communication channel through PCIE randomly hangs
- Fd and memory leakage in vaapi shim, both on IA host and on KMB ARM
- KMB ARM randomly kernel error. This happens randomly on KMB ARM and happens more often under heavy workload, e.g. 4-streams usecase
- h265 is only supported by decoder, but if the h265 file is of P010 pixel format, then the decoded frame is not supported by IE and Jpeg Encoder

------

------

###### OpenVINO dependency

Use [OpenVINO package released by ICV](http://nnt-srv01.inn.intel.com/builds/openvino_kmb/openvino-kmb.beta-20200422/builds/l_openvino_toolkit_private_ubuntu18_kmb_x86_p_0.0.0-2813-g509e8d4a56.tar.gz), or build HDDL2 plugin & InferenceEngine (`script/build_dldt.sh`) 

###### Boost

`sudo apt-get install libboost-all-dev`

###### Gstreamer

Currently we are using and testing under Gstreamer 1.16.0

Gstreamer Vaapi plugin should be get from IOTG Keembay repository rather than the open source version. Note that we should have a patch fixing on gstreamer vaapi dup issue, which is can be found at HddlUnite release page.

Another Gstreamer Plugin required to run the application is the bypass plugin, which is designed and written to handle the workload context and related data introduced specifically by bypass mode. To compile this plugin, one may refer to the GstBypassPlugin directory for its source code, or Hddlunite will also provide a script which is used to patch on the original version of bypass plugin present in Vaapi Shim repository and build it.

###### VAAPI Shim

Please use the latest commit on branch multisession, though the commit we tested upon is 83b441a4

###### Kmb IA GUI Application

Please use the latest commit on branch develop, though the commit we tested upon is a5fc637n