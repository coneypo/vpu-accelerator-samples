#ifndef _PIPELINESTATE_H_
#define _PIPELINESTATE_H_

#include <map>

#include "PipelineStatus.h"

namespace hddl {

enum class ReqType : int {
    CREATE,
    PLAY,
    PAUSE,
    MODIFY,
    STOP,
    DESTROY
};

enum class MPState : int {
    NONEXIST,
    CREATED,
    PLAYING,
    PAUSED,
    STOPPED,
    RUNTIME_ERROR,
    PIPELINE_EOS
};

const std::map<std::pair<ReqType, MPState>, PipelineStatus> constStateMap = {
    { { ReqType::CREATE, MPState::NONEXIST }, PipelineStatus::SUCCESS },
    { { ReqType::CREATE, MPState::CREATED }, PipelineStatus::ALREADY_CREATED },
    { { ReqType::CREATE, MPState::PLAYING }, PipelineStatus::ALREADY_CREATED },
    { { ReqType::CREATE, MPState::PAUSED }, PipelineStatus::ALREADY_CREATED },
    { { ReqType::CREATE, MPState::STOPPED }, PipelineStatus::ALREADY_CREATED },
    { { ReqType::CREATE, MPState::RUNTIME_ERROR }, PipelineStatus::RUNTIME_ERROR },
    { { ReqType::CREATE, MPState::PIPELINE_EOS }, PipelineStatus::PIPELINE_EOS },

    { { ReqType::PLAY, MPState::NONEXIST }, PipelineStatus::NOT_EXIST },
    { { ReqType::PLAY, MPState::CREATED }, PipelineStatus::SUCCESS },
    { { ReqType::PLAY, MPState::PLAYING }, PipelineStatus::ALREADY_STARTED },
    { { ReqType::PLAY, MPState::PAUSED }, PipelineStatus::SUCCESS },
    { { ReqType::PLAY, MPState::STOPPED }, PipelineStatus::STOPPED },
    { { ReqType::PLAY, MPState::RUNTIME_ERROR }, PipelineStatus::RUNTIME_ERROR },
    { { ReqType::PLAY, MPState::PIPELINE_EOS }, PipelineStatus::PIPELINE_EOS },

    { { ReqType::PAUSE, MPState::NONEXIST }, PipelineStatus::NOT_EXIST },
    { { ReqType::PAUSE, MPState::CREATED }, PipelineStatus::NOT_PLAYING },
    { { ReqType::PAUSE, MPState::PLAYING }, PipelineStatus::SUCCESS },
    { { ReqType::PAUSE, MPState::PAUSED }, PipelineStatus::NOT_PLAYING },
    { { ReqType::PAUSE, MPState::STOPPED }, PipelineStatus::NOT_PLAYING },
    { { ReqType::PAUSE, MPState::RUNTIME_ERROR }, PipelineStatus::RUNTIME_ERROR },
    { { ReqType::PAUSE, MPState::PIPELINE_EOS }, PipelineStatus::PIPELINE_EOS },

    { { ReqType::MODIFY, MPState::NONEXIST }, PipelineStatus::NOT_EXIST },
    { { ReqType::MODIFY, MPState::CREATED }, PipelineStatus::SUCCESS },
    { { ReqType::MODIFY, MPState::PLAYING }, PipelineStatus::SUCCESS },
    { { ReqType::MODIFY, MPState::PAUSED }, PipelineStatus::SUCCESS },
    { { ReqType::MODIFY, MPState::STOPPED }, PipelineStatus::STOPPED },
    { { ReqType::MODIFY, MPState::RUNTIME_ERROR }, PipelineStatus::RUNTIME_ERROR },
    { { ReqType::MODIFY, MPState::PIPELINE_EOS }, PipelineStatus::PIPELINE_EOS },

    { { ReqType::STOP, MPState::NONEXIST }, PipelineStatus::NOT_EXIST },
    { { ReqType::STOP, MPState::CREATED }, PipelineStatus::NOT_PLAYING },
    { { ReqType::STOP, MPState::PLAYING }, PipelineStatus::SUCCESS },
    { { ReqType::STOP, MPState::PAUSED }, PipelineStatus::SUCCESS },
    { { ReqType::STOP, MPState::STOPPED }, PipelineStatus::NOT_PLAYING },
    { { ReqType::STOP, MPState::RUNTIME_ERROR }, PipelineStatus::SUCCESS },
    { { ReqType::STOP, MPState::PIPELINE_EOS }, PipelineStatus::SUCCESS },

    { { ReqType::DESTROY, MPState::NONEXIST }, PipelineStatus::NOT_EXIST },
    { { ReqType::DESTROY, MPState::CREATED }, PipelineStatus::SUCCESS },
    { { ReqType::DESTROY, MPState::PLAYING }, PipelineStatus::SUCCESS },
    { { ReqType::DESTROY, MPState::PAUSED }, PipelineStatus::SUCCESS },
    { { ReqType::DESTROY, MPState::STOPPED }, PipelineStatus::SUCCESS },
    { { ReqType::DESTROY, MPState::RUNTIME_ERROR }, PipelineStatus::SUCCESS },
    { { ReqType::DESTROY, MPState::PIPELINE_EOS }, PipelineStatus::SUCCESS }
};
}
#endif //_PIPELINESTATE_H_
