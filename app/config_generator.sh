#!/bin/bash
set -e

AppModel="bypass"
PipelineNum=1

ByPassTemplate="filesrc location=\${media_file} ! qtdemux ! h264parse ! mfxh264dec ! inference syncmode=index socketname=\${socket_name} ! osdparser  ! mfxsink name=\${display_sink} sync=false"
StreamingTemplate="filesrc location=\${media_file} ! qtdemux ! h264parse ! tee name=t0 ! queue ! remoteoffloadbin.(vaapih264dec !  videoconvert ! gvadetect model=\${detection_model} ! queue ! gvawatermark ! videoconvert ! videoroimetadetach  ) ! roisink socketname=\${socket_name} sync=false t0. ! queue ! mfxh264dec ! inference syncmode=pts socketname=\${socket_name} ! osdparser  ! mfxsink name=\${display_sink} sync=false"

Seperator=","
ConfigPrefix="{"
ConfigSuffix="}"



DecodeConfig=""
HvaConfig=""
HvaSocket=""
HvaSocketConfig=""
AppConfig="\"app\":{\"timeout\":0, \"mode\":\"replay\"}"

declare -a VideoFileList 
HvaSocket=""
HvaCwd=""


function usage()
{
    echo "This script is used to generate config files for applications"
    echo "Usage config_generator.sh --num [pipelineNums] --model [bypass|streaming]"
}


function find_file_in_dir_list()
{

    dirList=$1
    fileName=$2

    oldIFS=$IFS
    IFS=:
    result=0
    for d in $dirList
    do
        [ -f "$d/$fileName" ] && result=1
    done
    IFS=$oldIFS
    echo $result
}

function check_file_exist()
{
    fileName=$1
    result=1
    if [[ -z $fileName || ! -f $fileName ]]; then
	result=0
    fi
    echo $result
}




function generate_decode_config()
{

    DecodePrefix="\"decode\":["
    DecodeSuffix="]"
    DecodeContext=""

    declare -a ChannelConfig
    for i in $(seq 1 $PipelineNum);
    do
        read -p "Input your gstreamer pipeline ${i}[press enter to use default template]:" PipelineStr 
        if [[ -z $PipelineStr ]]; then
            if [ $AppModel == "bypass" ]; then
                 PipelineStr=$ByPassTemplate
	    else
                 PipelineStr=$StreamingTemplate
	    fi
        fi
    
	res=0
	while [[ $res -ne 1 ]]
        do
            read -p  "Enter video file path:" VideoFilePath
	    res=$(check_file_exist $VideoFilePath)
	    if [ $res -ne 1 ]; then
                echo "Invalid video file path, please input it again!"
	    fi
	done

    
        ChannelParams="{\"media_file\": \"$VideoFilePath\",  \"socket_name\":\"/tmp/hddl_app_pipeline_$i.sock\", \"display_sink\":\"mysink\"}"
        ChannelConfig[$i]="{ \"pipe\": \"$PipelineStr\", \"param\": $ChannelParams }"
	    VideoFileList[$i]=$VideoFilePath
    
        if [ $i == 1 ]; then
            DecodeContext+="${ChannelConfig[$i]}"
        else
            DecodeContext+="$Seperator ${ChannelConfig[$i]}"
        fi
    done
    DecodeConfig="$DecodePrefix $DecodeContext $DecodeSuffix"
}

function generate_hva_field()
{
    HvaPrefix="\"hva\":{"
    HvaSuffix="}"
    res=0
    while [[ $res -ne 1 ]]
    do
        read -p "Enter your path to start hva pipeline[./FullPipe]:" HvaCmd 
        if [[ -z $HvaCmd ]]; then
            HvaCmd="./FullPipe"
        fi
	res=$(check_file_exist $HvaCmd)
	if [ $res -ne 1 ]; then
            echo "Can not find FullPipe, please input it again!"
	fi
    done
    HvaCmdConfig="\"cmd\": \"$HvaCmd\""

    read -p "Enter hva socket listenning for GUI messages. This will be consistent in generated config files[/tmp/gstreamer_ipc_second.sock]:" HvaSocket
    if [[ -z $HvaSocket ]]; then
        HvaSocket="/tmp/gstreamer_ipc_second.sock"
    fi
    HvaSocketConfig="\"socket_name\": \"$HvaSocket\""
    
    res=0
    while [[ $res -ne 1 ]]
    do
        read -p "Enter the working directory of hva pipeline. This is also the path containing config.json used by hva pipeline:" HvaCwd
        if [[ -d $HvaCwd ]]; then
	    res=1
	else
            echo "Can not find directory $HvaCwd, please input it again!"
	fi
    done
    HvaCwdConfig="\"work_directory\": \"$HvaCwd\""
    
    
    HvaEnvironmentPrefix="\"environment\": ["
    HvaEnvironmentSuffix="]"
    
    res=0
    while [[ $res -ne 1 ]]
    do
        read -p "set hva environment variable LIBVA_DRIVERS_PATH:" HvaLibvaPath 
	res=$(find_file_in_dir_list $HvaLibvaPath hddl_bypass_drv_video.so)
	if [ $res -ne 1 ]; then
            echo "Can not find hddl_bypass_drv_video.so, please input it again!"
	fi
    done
    HvaEnvironmentContext[0]="{\"key\": \"LIBVA_DRIVERS_PATH\",\"value\": \"$HvaLibvaPath\"}"

    res=0
    while [[ $res -ne 1 ]]
    do
        read -p "set hva environment variable GST_PLUGIN_PATH:" HvaGstPluginPath
	echo $HvaGstPluginPath
	res1=$(find_file_in_dir_list $HvaGstPluginPath libgstvaapi.so)
	res2=$(find_file_in_dir_list $HvaGstPluginPath libgstbypass.so)
	if [ $res1 -eq 1 ] && [ $res2 -eq 1 ];then
            res=1
        fi

	if [ $res -ne 1 ]; then
            echo "Can not find libgstvaapi.so/libgstbypass.so, please input it again!"
	fi
    done
    HvaEnvironmentContext[1]="{\"key\": \"GST_PLUGIN_PATH\",\"value\": \"$HvaGstPluginPath\"}"

    res=0
    while [[ $res -ne 1 ]]
    do
        read -p "set hva environment variable CONFIG_PATH, which is requested by vaapi shim and should contain connection.cfg in its value:" HvaConfigPath
	res=$(check_file_exist $HvaConfigPath)
	if [ $res -ne 1 ]; then
            echo "Can not find connection.cfg, please input it again!"
	fi
    done

    read -p "set hva environment variable LD_LIBRARY_PATH, which is the system search path for shared libraries at runtime:" HvaLdLibPath
    if [[ -z $HvaLdLibPath ]]; then
        echo "Invaild LD_LIBRARY_PATH"
        exit
    fi

    HvaEnvironmentContext[2]="{\"key\": \"CONFIG_PATH\",\"value\": \"$HvaConfigPath\"}"
    HvaEnvironmentContext[3]="{\"key\": \"GST_VAAPI_ALL_DRIVERS\",\"value\": 1}"
    HvaEnvironmentContext[4]="{\"key\": \"BYPASS_BATCH_MODE\",\"value\": 1}"
    HvaEnvironmentContext[5]="{\"key\": \"LIBVA_DRIVER_NAME\",\"value\": \"hddl_bypass\"}"
    HvaEnvironmentContext[6]="{\"key\": \"LD_LIBRARY_PATH\",\"value\": \"$HvaLdLibPath\"}"


    HvaEnvironmentConfig=""
    for i in "${HvaEnvironmentContext[@]}"
    do
        HvaEnvironmentConfig+="$i $Seperator"
    done

    HvaEnvironmentConfig="$HvaEnvironmentPrefix ${HvaEnvironmentConfig%?} $HvaEnvironmentSuffix"
    HvaConfig="$HvaPrefix $HvaCmdConfig $Seperator $HvaSocketConfig $Seperator $HvaCwdConfig $Seperator $HvaEnvironmentConfig  $HvaSuffix"
}

function generate_hva_config()
{
    HvaPrefix="{"
    HvaSuffix="}"


    res=0
    while [[ $res -ne 1 ]]
    do
        read -p "Enter Detection model path:" HvaDetectionModelPath
	res=$(check_file_exist $HvaDetectionModelPath)
	if [ $res -ne 1 ]; then
            echo "Invalid dectection model path, please input it again!"
	fi
    done
    HvaDetectionConfig="\"Detection\":{\"Model\": \"$HvaDetectionModelPath\"}"

    res=0
    while [[ $res -ne 1 ]]
    do
        read -p "Enter classification model path:" HvaClassificationModelPath
	res=$(check_file_exist $HvaClassificationModelPath)
	if [ $res -ne 1 ]; then
            echo "Invalid classification model path, please input it again!"
	fi
    done
    HvaClassificationConfig="\"Classification\":{\"Model\": \"$HvaClassificationModelPath\"}"
 

    HvaDecodeConfig="\"Decode\":["
    HvaGuiConfig="\"GUI\":{\"Socket\": \"$HvaSocket\"}"

    read -p "Enter FRC DropEveryXFrame value[3]:" HvaFRCDropEveryXFrame
    if [[ -z $HvaFRCDropEveryXFrame ]]; then
        HvaFRCDropEveryXFrame=3
    fi

    read -p "Enter FRC DropXFrame value[2]:" HvaFRCDropXFrame
    if [[ -z $HvaFRCDropXFrame ]]; then
        HvaFRCDropXFrame=2
    fi

    HvaFRCConfig="\"FRC\":{\"DropEveryXFrame\": $HvaFRCDropEveryXFrame, \"DropXFrame\": $HvaFRCDropXFrame}"


    for i in $(seq 1 $PipelineNum);
    do
        echo "set decoding configurations for video: ${VideoFileList[$i]}" 
        read -p "set DropEveryXFrame value occured at video source[4]:" HvaDropEveryXFrame
        if [[ -z $HvaDropEveryXFrame ]]; then
            HvaDropEveryXFrame=4
        fi

        read -p "set DropXFrame value occured at video source[0]:" HvaDropXFrame
        if [[ -z $HvaDropXFrame ]]; then
            HvaDropXFrame=0
        fi

	read -p "set codec, currently support h264, h265 or mp4 only[h264]:" HvaCodec
        if [[ -z $HvaCodec ]]; then
            HvaCodec="h264"
        fi
    
        DecodeParam="{\"Video\": \"${VideoFileList[$i]}\", \"DropEveryXFrame\":$HvaDropEveryXFrame, \"DropXFrame\":$HvaDropXFrame, \"Codec\":\"$HvaCodec\"}"
    
        if [ $i == 1 ]; then
            HvaDecodeConfig+="$DecodeParam"
        else
            HvaDecodeConfig+="$Seperator $DecodeParam"
        fi
    done
    HvaDecodeConfig+="]"

    HvaFileConfig="$HvaPrefix $HvaDetectionConfig $Seperator $HvaClassificationConfig $Seperator $HvaDecodeConfig $Seperator $HvaGuiConfig $Seperator $HvaFRCConfig $HvaSuffix"
    echo $HvaFileConfig | jq . > config_for_hva_sample.json
    echo "config_for_hva_sample.json is successfully created in. Please mannually copy it to \$HvaCwd as config.json"
}


if [[ "$1" == "" ]]; then
    usage
    exit
fi

while [ "$1" != "" ]; do
    case $1 in
        -n | --num )            shift
                                PipelineNum=$1
                                ;;
        -m | --model )          shift
                                AppModel=$1
                                ;;
        -h | --help )           usage
                                exit
                                ;;
        * )                     usage
                                exit 1
    esac
    shift
done


echo "This tool is used to created configuration files for hddl application"

if dpkg --get-selections | grep -q "^jq[[:space:]]*install$" >/dev/null; then
    echo "Found jq in system..."
else
    echo "jq was not installed. Now installing jq..."
    if sudo apt-get -qq install jq; then
        echo "Successfully installed jq"
    else
        echo "Failed to install jq. Exit"
        exit
    fi
fi

generate_decode_config

if [ "$AppModel" == "bypass" ]; then
    generate_hva_field
    generate_hva_config
    Config="$ConfigPrefix $DecodeConfig $Seperator $AppConfig $Seperator $HvaConfig $ConfigSuffix"
    # echo $Config
    echo $Config | jq . > config_generated.json
    echo "Hddl bypass application configuration config_generated.json is successfully created."

elif [ "$AppModel" == "streaming" ]; then
    Config="$ConfigPrefix $DecodeConfig $Seperator $AppConfig $ConfigSuffix"
    echo $Config | jq . > config_generated.json
    echo "Hddl streaming application configuration config_generated.json is successfully created."
fi
