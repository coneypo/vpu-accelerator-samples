/* * MIT License
 *
 * Copyright (c) 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
