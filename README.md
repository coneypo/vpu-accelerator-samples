# KMB sample (gstream decoding + HVA detection/classifcation on ARM)
### Update on 2019 Dec 11th
A sample Bypass Pipeline 

This sample runs on IA host, with decoding offloaded to KMB and IE (detection + classification) on CPU. Due to the fact that VAAPI Shim only supports single thread, single process and single pipeline at the moment. The number of Streams can only be configured to 1 (refer to the hash define in main). 

#### External dependencies
Gstreamer (for decoding)
Boost
VAAPI Shim
OpenVino

#### Configuration
Configure model path, video file in config.json, which will be found by applicaiton on ${pwd}

#### Run
```shell
./build/src/GstHvaSample
```

------

This is a initial version for hva/gstream based KMB sample on KMB ARM.

Gstreamer is used for decoding. Then NV12 video frame(output of decoding) is sent to HVA pipeline(detection+classifcation).



###### Build:

```shell
git clone ssh://git@gitlab.devtools.intel.com:29418/IOTG_ICO_Video_Optimization/gsthvasample.git
cd gsthvasample
git checkout Kezhen/dev_on_kmb_multi
mkdir build
cd build
cmake ..
make
```

###### Run:

1. copy "barrier_1080x720.h264", "resnet.labels", "build/src/GstHvaSample", "libhva.so" to KMB ARM, put in same directory

2. ```shell
   export LD_LIBRARY_PATH:$LD_LIBRARY_PATH:`pwd`
   ```

3. ```shell
   ./GstHvaSample
   ```

###### Todo:

In this version, there are many limitations such as parameter hard coding and lack of error handling. Besides, large stream number(8/16) is not tested, performance need to be optimized and tracking need to be added into pipeline.
