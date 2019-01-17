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
