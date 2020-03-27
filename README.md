# KMB sample (gstream decoding + HVA detection/classifcation on ARM)
### Update on 2019 Dec 11th
A sample Bypass Pipeline 

This sample runs on IA host, with decoding offloaded to KMB and IE (detection + classification) on VPU. Single process and single pipeline at the moment. The number of Streams can only be configured to 1 (refer to the hash define in main). 

#### External dependencies
- Gstreamer (for decoding)
- [Boost](#boost)
- VAAPI Shim
- [OpenVino](#openvino-dependency)

#### Build:

```shell
git clone ssh://git@gitlab.devtools.intel.com:29418/IOTG_ICO_Video_Optimization/gsthvasample.git
cd gsthvasample
git checkout ${branch/commit} #todo
mkdir build
cd build
cmake ..
make
```

#### Configuration
Configure model path, video file in config.json, which will be found by applicaiton on ${pwd}

#### Run
```shell
 #todo
```



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



