#ifndef __LOCAL_DEBUG_H__
#define __LOCAL_DEBUG_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

#define LOG_ERROR(format, ...)    \
    printf ("ERROR: " format "\n", ## __VA_ARGS__)

#define LOG_WARNING(format, ...)   \
    printf ("WARNING: " format "\n", ## __VA_ARGS__)

#ifndef DEBUG
#define LOG_DEBUG(format, ...)
#else
#define LOG_DEBUG(format, ...)   \
    printf ("DEBUG :" format "\n", ## __VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif
