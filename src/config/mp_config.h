/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <gst/gst.h>
#include "../utils/mp_utils.h"

extern const char *g_config_filename;
extern const char *g_launch_filename;
gboolean parse_cmdline(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif


#endif
