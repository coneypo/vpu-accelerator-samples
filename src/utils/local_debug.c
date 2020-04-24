/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "local_debug.h"


/* --------------------------------------------------------------------------*/
/**
 * @Debug Category intialization
 */
/* ----------------------------------------------------------------------------*/

gpointer init_debug_log(gpointer data)
{
    GST_DEBUG_CATEGORY_STATIC(mediapipe_debug);
    GST_DEBUG_CATEGORY_INIT (mediapipe_debug, "MEDIAPIPE", 0, "hddl_manager debug");
    return mediapipe_debug;
}

gpointer get_debug_flags (void)
{
    static GOnce debug_flag = G_ONCE_INIT;
    g_once (&debug_flag, init_debug_log, NULL);
    return debug_flag.retval;
}
