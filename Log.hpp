#ifndef _LOGGER_HPP_
#define _LOGGER_HPP_

#include <ctime>
#include <cstdio>
#include <thread>
#include <sstream>

// 日志宏颜色等级
#define RESET_COLOR "\033[0m"
#define BLUE_COLOR "\033[34m"
#define YELLOW_COLOR "\033[33m"
#define RED_COLOR "\033[31m"

// 日志等级
#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL INF

// 日志等级转字符串
#define LEVEL_TO_STRING(x) (\
    (x == INF) ? BLUE_COLOR "INF" RESET_COLOR : \
    (x == DBG) ? YELLOW_COLOR "DBG" RESET_COLOR : \
    (x == ERR) ? RED_COLOR "ERR" RESET_COLOR : "UNKNOWN" \
)

// 日志输出
#define LOG(level, format, ...) do { \
    if (level < LOG_LEVEL) break; \
    time_t t = time(NULL); \
    struct tm *ltm = localtime(&t); \
    char tmp[32] = {0}; \
    strftime(tmp, 31, "%Y-%m-%d %H:%M:%S", ltm); \
    std::ostringstream oss; \
    oss << std::this_thread::get_id(); \
    fprintf(stdout, "[%s %s %s:%d %s] " format "\n", oss.str().c_str(), tmp, __FILE__, __LINE__, LEVEL_TO_STRING(level), ##__VA_ARGS__); \
} while (0)

#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)

#endif // _LOGGER_HPP_
