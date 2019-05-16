#ifndef MEDIAPIPE2_PIPELINEEVENT_H
#define MEDIAPIPE2_PIPELINEEVENT_H

namespace hddl {
enum class PipelineEvent : int {
    PIPELINE_EOS = 1,
    RUNTIME_ERROR = 2
};
}
#endif //MEDIAPIPE2_PIPELINEEVENT_H
