# KMB sample (gstream decoding + HVA detection/classifcation on ARM)

A sample Bypass Pipeline

This sample runs on IA host, with decoding offloaded to KMB and IE (detection + classification) on VPU. Single process and single pipeline at the moment. The number of Streams can only be configured to 1 (refer to the hash define in main). 

#### External dependencies

- [Gstreamer (for decoding)](#gstreamer)
- [Boost](#boost)
- [VAAPI Shim](#vaapi-shim)
- [OpenVino](#openvino-dependency)
- [Kmb IA GUI Application](#kmb-ia-gui-application)

The sections that each entity points briefly introduces the requirements and version of each component. To facilitate users to set up the envrironment, some scripts are provided under `script/`. Below are the steps to set up the environment using the scripts provided.

1. build and install hddlunite. Hddlunite also provides a script of installation and hddlunite can be installed free of choice
2. make sure you have the access for
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
5. `script/build_vaapishim.sh -d 0 -k 0` where -d 0 refers to release build and -d 1 refers to debug build. -k 0 refers to to NOT build KMB binaries and libraries, to build and install vaapi shim
6. `script/build_gst.sh -d 0` to build and install gstreamers
7. `script/build_dldt.sh -d 0` to build and install dldt and hddl2plugins

#### Build

If you have already had all the above dependencies ready, to compile this sample, just simply do:

```shell
git clone https://gitlab.devtools.intel.com/kmb_integration/gsthvasample.git 
cd gsthvasample
git checkout feature/for_scan
```

Before carrying on the compilation, make sure you have the following environment variables configured properly, especially when you install all those dependencies to a custom location rather than the system root, i.e. `/usr/`.

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

##### if testing with GUI application

The GUI application will start HVA pipeline automatically. In order to make the GUI locate and boot HVA pipeline, a few configurations and environment variable should be set in GUI application's config file. An example GUI config file is provided under `external/`

#### Known Issue

- Vaapi and IE may experience stability issue after ~500 frames. Upon such failures the application should quit with appropriate printout on terminal
- Key frame tracking issue after loopback. Currently application is set to be looping until the GUI socket quits. If it reaches EOS of video file, application automatically seeks to the beginning of video file. However our tests show that after loopback the key frame tracking in Gstreamer pipeline fails to function well, resulting in blurred decoded image and subsequently inaccurate detection/classification result.
- in vaapi shim with commit id c6c2be32, vaapi fails to decode any frames with resolution above 1080p. Vaapi shim team should have fixed this issue in their latest commit but we havn't tested upon it.
- Fd and memory leakage in vaapi shim

------

------

###### OpenVINO dependency

Use OpenVINO package released by ICV, or build HDDL2 plugin & InferenceEngine (`script/build_dldt.sh`) 

###### Boost

`sudo apt-get install libboost-all-dev`

###### Gstreamer

Currently we are using and testing under Gstreamer 1.16.0

Gstreamer Vaapi plugin should be get from IOTG Keembay repository rather than the open source version. Note that we should have a patch fixing on gstreamer vaapi dup issue, which is can be found at HddlUnite release page.

Another Gstreamer Plugin required to run the application is the bypass plugin, which is designed and written to handle the workload context and related data introduced specifically by bypass mode. To compile this plugin, one may refer to the GstBypassPlugin directory for its source code, or Hddlunite will also provide a script which is used to patch on the original version of bypass plugin present in Vaapi Shim repository and build it. 

###### VAAPI Shim

We have conducted some smoke tests using vaapi shim commit id c6c2be32. With this commit vaapi shim has issues of corrupted fd when creating VA Surfaces from fd and fd leakages. We have come up with a patch fixing on them (corrupted fd, and partially fixed fd leakage). Vaapi shim team should have incorporate this patch in their latest commit but we have not tested upon it. If in any case that the latest vaapi shim fails to function well, users are advised to use the commit id mentioned with patch provided in Hddlunite release page.

###### Kmb IA GUI Application

This application is tested together with Kmb IA Sample with commit id d21386e1
