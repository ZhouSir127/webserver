#ifndef LOG_H
#define LOG_H

#include <cstdio>
#include <cstdarg>
#include <string>
#include <mutex>
#include <sys/time.h>

class Log {
public:
    // 禁用构造函数，表明这是一个纯静态工具类，不允许任何人 new 它
    Log() = delete;

    // 全局初始化：仅在 main 函数中调用一次
    static void init(const std::string& file_name, bool close_log);
    
    // 全局写日志接口
    static void write_log(int level, const char* format, ...);
    
    // 优雅关闭：在程序退出前调用（可选）
    static void close();

private:
    // 静态成员变量（相当于被类名作用域保护的全局变量）
    static FILE* s_fp;
    static std::mutex s_mutex;
    static bool s_close_log;
};


#define LOG_DEBUG(format, ...) Log::write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  Log::write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)  Log::write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::write_log(3, format, ##__VA_ARGS__)

#endif // LOG_H