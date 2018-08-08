#ifndef __LOCAL_DEBUG_H__
#define __LOCAL_DEBUG_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

#define RED_BEGIN "\033[31m"
#define YELLOW_BEGIN "\033[33m"
#define GREEN_BEGIN "\033[32m"
#define CORLOR_END   "\033[0m"

#define LOG_ERROR(format, ...)    \
    printf ("%s[ERROR] " format "%s\n", RED_BEGIN, ## __VA_ARGS__, CORLOR_END)

#define LOG_WARNING(format, ...)   \
    printf ("%s[WARNING] " format "%s\n", YELLOW_BEGIN, ## __VA_ARGS__, CORLOR_END)

#define LOG_INFO(format, ...)   \
    printf ("%s[INFO] " format "%s\n", GREEN_BEGIN, ## __VA_ARGS__, CORLOR_END)

#ifndef DEBUG
#define LOG_DEBUG(format, ...)
#else
#define LOG_DEBUG(format, ...)   \
    printf ("[DEBUG] " format "\n", ## __VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif
