#ifndef _PIPELINESTATUS_H_
#define _PIPELINESTATUS_H_

namespace hddl {

enum class PipelineStatus : int {
    SUCCESS = 0,
    PIPELINE_EOS = 1,
    RUNTIME_ERROR = 2,
    ERROR = -1,
    COMM_TIMEOUT = -2,
    INVALID_PARAMETER = -3,
    NOT_EXIST = -4,
    ALREADY_CREATED = -5,
    ALREADY_STARTED = -6,
    NOT_PLAYING = -7,
    STOPPED = -8,
    INVALID_DST_PATH = -9,
    FILE_ALREADY_EXIST = -10
};

enum class PipelineEvent : int {
    PIPELINE_EOS = 1,
    RUNTIME_ERROR = 2
};
}

#endif // _PIPELINESTATUS_H_
