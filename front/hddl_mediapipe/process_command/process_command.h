#ifndef __PROCESS_COMMAND_H__
#define __PROCRSS_COMMAND_H__

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
};


gboolean create_pipeline(char *desc, mediapipe_t *mp);
void set_property(json_object *desc, mediapipe_t *mp);

#ifdef __cplusplus
}
#endif

#endif
