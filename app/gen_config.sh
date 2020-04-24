#!/bin/bash

AppModel="bypass"
PipelineNum=1

ByPassTemplate="filesrc location=\${media_file} ! h264parse ! mfxh264dec ! inference syncmode=index socketname=\${socket_name} ! osdparser  ! mfxsink name=\${display_sink} sync=false"
StreamingTemplate="filesrc location=\${media_file} ! h264parse ! mfxh264dec ! inference syncmode=index socketname=${socket_name} ! osdparser  ! mfxsink name=${display_sink} sync=false"

Seperator=","
ConfigPrefix="{"
ConfigSuffix="}"



DecodeConfig=""
HvaConfig=""
AppConfig="\"app\":{\"timeout\":0, \"mode\":\"replay\"}"



usage()
{
    echo "usage gen_config --num [pipelineNums] --model [bypass|streaming]"
}


generate_decode_config()
{

    DecodePrefix="\"decode\":["
    DecodeSuffix="]"
    DecodeContext=""

    #echo "Gstreamer default pipeline for bypass mode: ${ByPassTemplate}"
    declare -a ChannelConfig
    for i in $(seq 1 $PipelineNum);
    do
        read -p "Input your gstreamer pipeline ${i}:[press enter to use default]" PipelineStr 
        if [[ -z $PipelineStr ]]; then
            PipelineStr=$ByPassTemplate
        fi
    
        read -p  "Enter video file path:" VideoFilePath
        if [[ -z $VideoFilePath || ! -f $VideoFilePath ]]; then
            echo "Invalid video file path!"
            exit
        fi
    
        ChannelParams="{\"media_file\": \"$VideoFilePath\",  \"socket_name\":\"/temp/hddl_app_pipeline_$i.sock\", \"display_sink\":\"mysink\"}"
        ChannelConfig[$i]="{ \"pipe\": \"$PipelineStr\", \"param\": $ChannelParams }"
    
        if [ $i == 1 ]; then
            DecodeContext+="${ChannelConfig[$i]}"
        else
            DecodeContext+="$Seperator ${ChannelConfig[$i]}"
        fi
    done
    DecodeConfig="$DecodePrefix $DecodeContext $DecodeSuffix"
}

generate_hva_config()
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
    if [[ -z $HvaLibvaPath || ! -d $HvaLibvaPath ]]; then
        echo "Invaild LIBVA_DRIVERS_PATH value"
        exit
    fi
    HvaEnvironmentContext[0]="{\"key\": \"LIBVA_DRIVERS_PATH\",\"value\": \"$HvaLibvaPath\"}"

    
    read -p "set hva environment variable GST_PLUGIN_PATH:" HvaGstPluginPath
    if [[ -z $HvaGstPluginPath || ! -d $HvaGstPluginPath ]]; then
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




generate_decode_config

generate_hva_config

Config="$ConfigPrefix $DecodeConfig $Seperator $AppConfig $Seperator $HvaConfig	$ConfigSuffix"


echo $Config | jq . > config_generated.json
