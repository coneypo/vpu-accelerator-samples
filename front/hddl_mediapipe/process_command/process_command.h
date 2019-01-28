#ifndef __PROCESS_COMMAND_H__
#define __PROCRSS_COMMAND_H__

#include "mediapipe.h"


#ifdef __cplusplus
extern "C" {
#endif


enum E_COMMAND_TYPE {
    eCommand_None = -1,
    eCommand_PipeCreate = 0,
    eCommand_Pipeid = 1,
    eCommand_Config = 2,
    eCommand_Launch = 3,
    eCommand_SetProperty = 4,
    eCommand_PipeDestroy = 5,
    eCommand_Metadata = 6
};

gboolean process_command(mediapipe_t *mp, void *message);

#ifdef __cplusplus
}
#endif

#endif
