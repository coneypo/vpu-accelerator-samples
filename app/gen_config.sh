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
HvaSocketConfig=""
AppConfig="\"app\":{\"timeout\":0, \"mode\":\"replay\"}"

declare -a VideoFileList 
HvaSocket=""
HvaCwd=""


usage()
{
    echo "usage gen_config --num [pipelineNums] --model [bypass|streaming]"
}


generate_decode_config()
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
    
        read -p  "Enter video file path:" VideoFilePath
        if [[ -z $VideoFilePath || ! -f $VideoFilePath ]]; then
            echo "Invalid video file path!"
            exit
        fi
    
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

generate_hva_field()
{
    HvaPrefix="\"hva\":{"
    HvaSuffix="}"
    
    read -p "Enter hva cmd[./FullPipe]:" HvaCmd 
    if [[ -z $HvaCmd ]]; then
        HvaCmd="./FullPipe"
    fi
    HvaCmdConfig="\"cmd\": \"$HvaCmd\""
    
    
    read -p "Enter hva socket name[/tmp/hva_ipc.sock]:" HvaSocket
    if [[ -z $HvaSocket ]]; then
        HvaSocket="/tmp/hva_ipc.sock"
    fi
    HvaSocketConfig="\"socket_name\": \"$HvaSocket\""
    
    
    read -p "Enter hva working directory:" HvaCwd
    if [[ -z $HvaCwd || ! -d $HvaCwd ]]; then
        echo "Invaild hva working directory"
        exit
    fi
    HvaCwdConfig="\"working_directory\": \"$HvaCwd\""
    
    
    HvaEnvironmentPrefix="\"environment\": ["
    HvaEnvironmentSuffix="]"
    
    
    read -p "set hva environment variable LIBVA_DRIVERS_PATH:" HvaLibvaPath 
    if [[ -z $HvaLibvaPath || ! -d $HvaLibvaPath || ! -f $HvaLibvaPath/hddl_bypass_drv_video.so ]]; then
        echo "Invaild LIBVA_DRIVERS_PATH value"
        exit
    fi
    HvaEnvironmentContext[0]="{\"key\": \"LIBVA_DRIVERS_PATH\",\"value\": \"$HvaLibvaPath\"}"

    
    read -p "set hva environment variable GST_PLUGIN_PATH:" HvaGstPluginPath
    if [[ -z $HvaGstPluginPath || ! -d $HvaGstPluginPath || ! -f $HvaGstPluginPath\libgstvaapi.so || ! -f $HvaGstPluginPath\libgstbypass.so ]]; then
        echo "Invaild GST_PLUGIN_PATH"
        exit
    fi
    HvaEnvironmentContext[1]="{\"key\": \"LIBVA_DRIVERS_PATH\",\"value\": \"$HvaGstPluginPath\"}"
    
    
    read -p "set hva environment variable CONFIG_PATH:" HvaConfigPath
    if [[ -z $HvaConfigPath || ! -f $HvaConfigPath/connection.cfg ]]; then
        echo "Invaild CONFIG_PATH"
        exit
    fi
    HvaEnvironmentContext[2]="{\"key\": \"LIBVA_DRIVERS_PATH\",\"value\": \"$HvaConfigPath\"}"
    HvaEnvironmentContext[3]="{\"key\": \"GST_VAAPI_ALL_DRIVERS\",\"value\": 1}"
    HvaEnvironmentContext[4]="{\"key\": \"BYPASS_BATCH_MODE\",\"value\": 1}"

    HvaEnvironmentConfig=""
    for i in "${HvaEnvironmentContext[@]}"
    do
        HvaEnvironmentConfig+="$i $Seperator"
    done

    HvaEnvironmentConfig="$HvaEnvironmentPrefix ${HvaEnvironmentConfig%?} $HvaEnvironmentSuffix"
    HvaConfig="$HvaPrefix $HvaCmdConfig $Seperator $HvaSocketConfig $Seperator $HvaCwdConfig $Seperator $HvaEnvironmentConfig  $HvaSuffix"
}

generate_hva_config()
{
    HvaPrefix="{"
    HvaSuffix="}"

    read -p "Enter Detection model path:" HvaDetectionModelPath
    if [[ -z $HvaDetectionModelPath || ! -f $HvaDetectionModelPath ]]; then
        echo "Invaild detection model path"
        exit
    fi
    HvaDetectionConfig="\"Detection\":{\"Model\": \"$HvaDetectionModelPath\"}"

    read -p "Enter classification model path:" HvaClassificationModelPath
    if [[ -z $HvaClassificationModelPath || ! -f $HvaClassificationModelPath ]]; then
        echo "Invaild classification model path"
        exit
    fi
    HvaClassificationConfig="\"Classification\":{\"Model\": \"$HvaClassificationModelPath\"}"
 

    HvaDecodeConfig="\"Decode\":["

    HvaGuiConfig="\"GUI\":{\"Socket\": \"$HvaSocket\"}"


    read -p "Enter FRC DropEveryXFrame value[4]:" HvaFRCDropEveryXFrame
    if [[ -z $HvaFRCDropEveryXFrame ]]; then
        HvaFRCDropEveryXFrame=4
    fi

    read -p "Enter FRC DropXFrame value[4]:" HvaFRCDropXFrame
    if [[ -z $HvaFRCDropXFrame ]]; then
        HvaFRCDropXFrame=4
    fi

    HvaFRCConfig="\"FRC\":{\"DropEveryXFrame\": $HvaFRCDropEveryXFrame, \"DropXFrame\": $HvaFRCDropXFrame}"


    for i in $(seq 1 $PipelineNum);
    do
        echo "Enter decoding configurations for video ${VideoFileList[$i]}" 
        read -p "set DropEveryXFrame value[4]:" HvaDropEveryXFrame
        if [[ -z $HvaDropEveryXFrame ]]; then
            HvaDropEveryXFrame=4
        fi

        read -p "Enter DropXFrame value[0]:" HvaDropXFrame
        if [[ -z $HvaDropXFrame ]]; then
            HvaDropXFrame=0
        fi

        read -p "Enter codec[h264]:" HvaCodec
        if [[ -z $HvaCodec ]]; then
            HvaCodec=0
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
    echo $HvaFileConfig
    echo $HvaFileConfig | jq . > $HvaCwd/config.json
    echo "Hva config config.json is successfully created in $HvaCwd."
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
generate_decode_config

if [ "$AppModel" == "bypass" ]; then
    generate_hva_field
    generate_hva_config
    Config="$ConfigPrefix $DecodeConfig $Seperator $AppConfig $Seperator $HvaConfig $ConfigSuffix"
    echo $Config | jq . > config_generated.json
    echo "Hddl bypass application configuration config_generated.json is successfully created."

elif [ "$AppModel" == "streaming" ]; then
    Config="$ConfigPrefix $DecodeConfig $Seperator $AppConfig $ConfigSuffix"
    echo $Config | jq . > config_generated.json
    echo "Hddl streaming application configuration config_generated.json is successfully created."
fi
