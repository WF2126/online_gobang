#ifndef _M_LOGGER_HPP_
#define _M_LOGGER_HPP_
#include <stdio.h>
#include <time.h>

// 追踪日志
#define INF 0
// 调试日志
#define DBG 1
// 错误日志
#define ERR 2

#define DEFAULT_LOG_LEVEL INF
#define LOG(level, format, ...) do{\
    if (DEFAULT_LOG_LEVEL > level) break;\
    time_t t = time(NULL);\
    struct tm *lt = localtime(&t);\
    char buf[32] = {0};\
    strftime(buf, 31, "%H:%M:%S", lt);\
    fprintf(stdout, "[%s %s:%d] " format "\n", buf, __FILE__, __LINE__, ##__VA_ARGS__);\
}while(0)

#define ILOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DLOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ELOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)

#endif// log.hpp