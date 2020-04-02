# KMB sample (gstream decoding + HVA detection/classifcation on ARM)

A sample Bypass Pipeline 

This sample runs on IA host, with decoding offloaded to KMB and IE (detection + classification) on VPU. Single process and single pipeline at the moment. The number of Streams can only be configured to 1 (refer to the hash define in main). 

#### External dependencies
- [Gstreamer (for decoding)](#gstreamer)
- [Boost](#boost)
- [VAAPI Shim](#vaapi-shim)
- [OpenVino](#openvino-dependency)
- [Kmb IA GUI Application](#kmb-ia-gui-application)

#### Build:

```shell
git clone https://gitlab.devtools.intel.com/kmb_integration/gsthvasample.git 
cd gsthvasample
git checkout feature/WW13.5
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
` ./build/src/FullPipe/FullPipe`
3. Wait until the application prints out "Set socket to listening"
4. Start the fake GUI testcase, e.g.
` ./build/src/FullPipeGUITest/FullPipeGUITest`
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

HDDL2 plugin was not part of OpenVINO regular build. A separate binary need to be installed manually.

How to install OpenVINO with HDDL2 plugin:

1. Untar OpenVINO package for x86
        http://nnt-srv01.inn.intel.com/builds/openvino_kmb/openvino-kmb.beta-20200313/builds/l_openvino_toolkit_private_ubuntu18_kmb_x86_p_0.0.0-2813-g509e8d4a56.tar.gz

2. Clone dldt repo

   `git clone git@gitlab-icv.inn.intel.com:inference-engine/dldt.git`

3. build dldt

   `cd ${dldt}; bash build-after-clone.sh `

4. copy(do not overwrite libraries in target path) libraries from `${dldt}/bin/intel64/Release/lib/*` to untared OpenVINO package `${openvino}/deployment_tools/inference_engine/lib/intel64/`

5. Untar HDDL2 plugin into `plugins.xml` path (`${openvino}/deployment_tools/inference_engine/lib/intel64/`)
        http://nnt-srv01.inn.intel.com/builds/openvino_kmb/openvino-kmb.beta-20200313/builds/HDDL2Plugin.tar.xz

6. Add HDDL2 plugin file name into plugins XML file `plugins.xml`.

7. Setup OpenVINO environment variable before use `source ${openvino}/bin/setupvars.sh`

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
