#include "log.h"
# include <stdarg.h>
// 【极其重要】：静态成员变量必须在 .cpp 文件中进行定义和初始化！
FILE* Log::s_fp = nullptr;
std::mutex Log::s_mutex;
bool Log::s_close_log = false;

void Log::init(const std::string& file_name, bool close_log) {
    s_close_log = close_log;
    
    if (s_close_log) {
        return; // 如果配置了关闭日志，直接返回，不打开文件
    }

    // 加锁，防止多线程环境下被重复初始化
    std::lock_guard<std::mutex> lock(s_mutex);
    
    // 如果之前已经打开过文件，先关闭（防止内存/句柄泄漏）
    if (s_fp != nullptr) {
        fclose(s_fp);
        s_fp = nullptr;
    }
    
    s_fp = fopen(file_name.c_str(), "a");
}

void Log::write_log(int level, const char* format, ...) {
    // 锁外判断，极致的性能优化：如果关闭了日志或文件未打开，直接丢弃
    if (s_close_log || s_fp == nullptr) {
        return;
    }

    // 1. 获取当前时间
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm* sys_tm = localtime(&t);

    // 2. 确定日志级别前缀
    const char* level_str = "[info]: ";
    if (level == 0) level_str = "[debug]: ";
    else if (level == 2) level_str = "[warn]: ";
    else if (level == 3) level_str = "[erro]: ";

    // 3. 锁住文件 I/O 区域，防止多线程日志内容交错混杂
    std::lock_guard<std::mutex> lock(s_mutex);

    // 打印时间和级别
    fprintf(s_fp, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
            sys_tm->tm_year + 1900, sys_tm->tm_mon + 1, sys_tm->tm_mday,
            sys_tm->tm_hour, sys_tm->tm_min, sys_tm->tm_sec, now.tv_usec, level_str);

    // 打印用户格式化的具体内容
    va_list valst;
    va_start(valst, format);
    vfprintf(s_fp, format, valst);
    va_end(valst);

    // 换行并强制刷入磁盘
    fprintf(s_fp, "\n");
    fflush(s_fp); 
}

void Log::close() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_fp != nullptr) {
        fclose(s_fp);
        s_fp = nullptr;
    }
}