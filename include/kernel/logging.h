#ifndef _KLOG_H
#define _KLOG_H

#include <stdlib.h>

/* Depending on this value, each logging function
 * (p_error, p_warn, p_info, p_debug, p_trace) outputs
 * log messages to the console, or does nothing.
 * LOG_LEVEL  p_error  p_warn  p_info  p_debug  p_trace
 *   0        o        x       x        x        x
 *   1        o        o       x        x        x
 *   2        o        o       o        x        x
 *   3        o        o       o        o        x
 *   4        o        o       o        o        o
 *
 * Here:
 *  o: Output log messages
 *  x: Do nothing
 */
         
#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif

#define LOG_ERROR 0
#define LOG_WARN  1
#define LOG_INFO  2
#define LOG_DEBUG 3
#define LOG_TRACE 4

#if LOG_LEVEL >= LOG_ERROR
#define p_error(tag, fmt, ...) debugln("[ERROR] [%s] " fmt, tag, ##__VA_ARGS__)
#else
#define p_error(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_WARN
#define p_warn(tag, fmt, ...) debugln("[WARN] [%s] " fmt, tag, ##__VA_ARGS__)
#else
#define p_warn(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_INFO
#define p_info(tag, fmt, ...) debugln("[INFO] [%s] " fmt, tag, ##__VA_ARGS__)
#else
#define p_info(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_DEBUG
#define p_debug(tag, fmt, ...) debugln("[DEBUG] [%s] " fmt, tag, ##__VA_ARGS__)
#else
#define p_debug(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_TRACE
#define p_trace(tag, fmt, ...) debugln("[TRACE] [%s] " fmt, tag, ##__VA_ARGS__)
#else
#define p_trace(tag, fmt, ...) ((void)0)
#endif

#endif
