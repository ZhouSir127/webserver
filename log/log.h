#ifndef LOG_H
#define LOG_H

#include <cstdio>
#include <string>
#include <mutex>
#include <sys/time.h>
#include <queue>
#include "../args.h"
#include <vector>
#include <chrono>   // C++ 时间库
#include <ctime>    // 本地时间转换
#include <condition_variable>
#include <thread> 

namespace myLog{

    extern std::vector<std::string>levelVec;
    extern FILE* fp;
    extern std::mutex lock;
    extern std::condition_variable cv;
    extern bool closeLog;
    extern std::queue<std::string> logQueue;
    extern bool isRunning;
    extern std::thread bgThread;
    
    void init(const LogInfo&logInfo);
    
    template<typename... Args>
    void write(int level,Args&&... args){
        if (closeLog || fp == nullptr)
            return;
        
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        tm* tm = std::localtime(&t);

        std::ostringstream oss;
        oss << std::setfill('0');

    // 拼接：年-月-日 时:分:秒
        oss << std::setw(4) << (tm->tm_year + 1900) << "-"
        << std::setw(2) << (tm->tm_mon + 1) << "-"
        << std::setw(2) << tm->tm_mday << " "
        << std::setw(2) << tm->tm_hour << ":"
        << std::setw(2) << tm->tm_min << ":"
        << std::setw(2) << tm->tm_sec << " "
        << levelVec[level];
        
        (oss << ... << std::forward<Args>(args));    
        oss << "\n";

        std::unique_lock<std::mutex>Lock(lock);
        logQueue.push(oss.str());
        cv.notify_one();
    }

    void close();
    void asyncWriter();
    
};

#define LOG_DEBUG(...)  myLog::write(0, ##__VA_ARGS__)
#define LOG_INFO(...)   myLog::write(1, ##__VA_ARGS__)
#define LOG_WARN(...)   myLog::write(2, ##__VA_ARGS__)
#define LOG_ERROR(...)  myLog::write(3, ##__VA_ARGS__)

#endif // LOG_H