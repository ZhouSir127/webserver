#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <mutex>
#include <time.h>
#include <sys/time.h>

class Log {
public:

    void write_log(int level, const char* format, ...) {
        if (m_close_log == 1 || m_fp == nullptr) {
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

        // 3. 锁住这块区域，防止多线程把日志打印串行
        std::lock_guard<std::mutex> lock(m_mutex);

        // 打印时间和级别
        fprintf(m_fp, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                sys_tm->tm_year + 1900, sys_tm->tm_mon + 1, sys_tm->tm_mday,
                sys_tm->tm_hour, sys_tm->tm_min, sys_tm->tm_sec, now.tv_usec, level_str);

        // 打印用户格式化的具体内容
        va_list valst;
        va_start(valst, format);
        vfprintf(m_fp, format, valst);
        va_end(valst);

        // 换行并强制刷入磁盘
        fprintf(m_fp, "\n");
        fflush(m_fp); 
    }

    Log(const std::string& file_name,int close_log) :m_fp(fopen(file_name.data(), "a")),m_close_log(close_log){}
    ~Log() { if (m_fp) fclose(m_fp); }

private:

    FILE* m_fp;
    std::mutex m_mutex;
    int m_close_log;
};

// 宏定义保持不变，完美兼容你原本项目里到处写的 LOG_INFO
#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  Log::get_instance()->write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)  Log::get_instance()->write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__)

#endif