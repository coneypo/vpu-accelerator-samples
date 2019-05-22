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
    DESTROY,
    SET_CHANNEL
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

extern const std::map<std::pair<ReqType, MPState>, PipelineStatus> stateMap;
}
#endif //_PIPELINESTATE_H_
