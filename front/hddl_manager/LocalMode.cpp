/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <iostream>
#include <json-c/json.h>
#include <thread>

#include "FileUtils.h"
#include "LocalMode.h"

namespace hddl {
int LocalMode::init(std::string& localFile, hddl::PipelineManager& pipeMgr, int seconds)
{
    m_pipeManager = &pipeMgr;
    m_runSeconds = seconds;

    std::string localData;
    if (readFile(localFile, localData))
        return -1;

    if (parseJson(localData)) {
        std::cerr << "Error: parse json file error." << std::endl;
        return -1;
    }

    return 0;
}

void LocalMode::uninit()
{
    m_pipeManager = nullptr;
}

void LocalMode::run()
{
    launchStartPipelines();
    std::this_thread::sleep_for(std::chrono::seconds(m_runSeconds));
    stopDestroyPipelines();
}

void LocalMode::launchStartPipelines()
{
    for (auto& info : m_pipeInfo) {
        for (int i = 0; i < info.threads; i++) {
            auto status = m_pipeManager->addPipeline(info.launchData, info.configData, m_pipeId);
            if (status != PipelineStatus::SUCCESS) {
                std::cerr << "Error: launch pipeline failed" << std::endl;
                stopDestroyPipelines();
                return;
            }

            m_pipeLaunched.emplace(m_pipeId, false);

            status = m_pipeManager->playPipeline(m_pipeId);
            if (status != PipelineStatus::SUCCESS) {
                std::cerr << "Error: play pipeline failed" << std::endl;
                stopDestroyPipelines();
                return;
            }
            m_pipeLaunched[m_pipeId++] = true;
        }
    }
}

void LocalMode::stopDestroyPipelines()
{
    for (auto iter = m_pipeLaunched.begin(); iter != m_pipeLaunched.end(); iter++) {
        if (iter->second) {
            auto status = m_pipeManager->stopPipeline(iter->first);
            if (status == PipelineStatus::PIPELINE_EOS) {
                std::cout << "pipeline eos" << std::endl;
            } else if (status == PipelineStatus::RUNTIME_ERROR) {
                std::cerr << "Pipeline runtime error occur" << std::endl;
            } else if (status != PipelineStatus::SUCCESS) {
                std::cerr << "Error: stop pipeline failed" << std::endl;
            }
        }

        auto status = m_pipeManager->deletePipeline(iter->first);
        if (status != PipelineStatus::SUCCESS) {
            std::cerr << "Error: destroy pipeline failed" << std::endl;
        }
    }
    m_pipeLaunched.clear();
}

int LocalMode::readFile(std::string& localFile, std::string& localData)
{
    if (!FileUtils::exist(localFile)) {
        std::cerr << "Error: local file " << localFile << " doesn't exist." << std::endl;
        return -1;
    }

    localData = FileUtils::readFile(localFile);
    if (localData.empty()) {
        std::cerr << "Error: read file error." << std::endl;
        return -1;
    }

    return 0;
}

int LocalMode::parseJson(std::string& localData)
{
    enum json_tokener_error jerr = json_tokener_success;
    json_object* root = json_tokener_parse_verbose(localData.c_str(), &jerr);
    if (jerr != json_tokener_success)
        return -1;

    json_object* local;
    if (!json_object_object_get_ex(root, "local_mode", &local))
        return -1;
    if (!json_object_is_type(local, json_type_array))
        return -1;

    std::string name, file;
    json_object *item, *value;
    json_object_iterator iter, end;
    int len = json_object_array_length(local);
    for (int i = 0; i < len; i++) {
        Info info;
        item = json_object_array_get_idx(local, i);
        end = json_object_iter_end(item);
        iter = json_object_iter_begin(item);

        if (json_object_iter_equal(&iter, &end))
            continue;

        // read launch
        name = json_object_iter_peek_name(&iter);
        if (name != "launch")
            return -1;
        value = json_object_iter_peek_value(&iter);
        if (!json_object_is_type(value, json_type_string))
            return -1;

        file = json_object_get_string(value);
        if (readFile(file, info.launchData))
            return -1;

        json_object_iter_next(&iter);
        if (json_object_iter_equal(&iter, &end))
            return -1;

        // read config
        name = json_object_iter_peek_name(&iter);
        if (name != "config")
            return -1;
        value = json_object_iter_peek_value(&iter);
        if (!json_object_is_type(value, json_type_string))
            return -1;

        file = json_object_get_string(value);
        if (readFile(file, info.configData))
            return -1;

        json_object_iter_next(&iter);
        if (json_object_iter_equal(&iter, &end))
            return -1;

        //read num
        name = json_object_iter_peek_name(&iter);
        if (name != "num")
            return -1;
        value = json_object_iter_peek_value(&iter);
        if (!json_object_is_type(value, json_type_int))
            return -1;

        info.threads = json_object_get_int(value);

        m_pipeInfo.push_back(std::move(info));
    }

    return 0;
}
}
