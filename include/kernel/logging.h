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

#define LOG_RED     "\033[1;31m"
#define LOG_YELLOW  "\033[1;33m"
#define LOG_GREEN   "\033[1;32m"
#define LOG_CYAN    "\033[1;36m"
#define LOG_MAGENTA "\033[1;35m"
#define LOG_RESET   "\033[0m"

#if LOG_LEVEL >= LOG_ERROR
#define p_error(tag, fmt, ...) debugln(LOG_RED "[ERROR]" LOG_RESET " (%s) " fmt, tag, ##__VA_ARGS__)
#else
#define p_error(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_WARN
#define p_warn(tag, fmt, ...) debugln(LOG_YELLOW "[WARN]" LOG_RESET " (%s) " fmt, tag, ##__VA_ARGS__)
#else
#define p_warn(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_INFO
#define p_info(tag, fmt, ...) debugln(LOG_GREEN "[INFO]" LOG_RESET " (%s) " fmt, tag, ##__VA_ARGS__)
#else
#define p_info(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_DEBUG
#define p_debug(tag, fmt, ...) debugln(LOG_CYAN "[DEBUG]" LOG_RESET " (%s) " fmt, tag, ##__VA_ARGS__)
#else
#define p_debug(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_TRACE
#define p_trace(tag, fmt, ...) debugln(LOG_MAGENTA "[TRACE]" LOG_RESET " (%s) " fmt, tag, ##__VA_ARGS__)
#else
#define p_trace(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_ERROR
#define vp_error(tag, fmt, args) vdebugln(LOG_RED "[ERROR]" LOG_RESET, tag, fmt, args)
#else
#define vp_error(tag, fmt, args) ((void)0)
#endif

#if LOG_LEVEL >= LOG_WARN
#define vp_warn(tag, fmt, args) vdebugln(LOG_YELLOW "[WARN]" LOG_RESET, tag, fmt, args)
#else
#define vp_warn(tag, fmt, args) ((void)0)
#endif

#if LOG_LEVEL >= LOG_INFO
#define vp_info(tag, fmt, args) vdebugln(LOG_GREEN "[INFO]" LOG_RESET, tag, fmt, args)
#else
#define vp_info(tag, fmt, args) ((void)0)
#endif

#if LOG_LEVEL >= LOG_DEBUG
#define vp_debug(tag, fmt, args) vdebugln(LOG_CYAN "[DEBUG]" LOG_RESET, tag, fmt, args)
#else
#define vp_debug(tag, fmt, args) ((void)0)
#endif

#if LOG_LEVEL >= LOG_TRACE
#define vp_trace(tag, fmt, args) vdebugln(LOG_MAGENTA "[TRACE]" LOG_RESET, tag, fmt, args)
#else
#define vp_trace(tag, fmt, args) ((void)0)
#endif

#endif
