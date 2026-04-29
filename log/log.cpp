#include "log.h"
# include <stdarg.h>


std::vector<std::string> myLog::levelVec{"[debug]: ","[warn]: ","[erro]: ","[info]: "};
FILE* myLog::fp = nullptr;
std::mutex myLog::lock;
std::condition_variable myLog::cv;
bool myLog::closeLog;
std::queue<std::string> myLog::logQueue;
bool myLog::isRunning;
std::thread myLog::bgThread;

void myLog::init(const LogInfo&logInfo) {    
    
    if ( (closeLog = logInfo.close) )
        return; // 如果配置了关闭日志，直接返回，不打开文件
    
    fp = fopen(logInfo.file.c_str(), "a");
    isRunning = true;
    bgThread = std::thread(asyncWriter);
}

void myLog::close() {
    isRunning = false;
    if (fp != nullptr) {
        fclose(fp);
        fp = nullptr;
    }
}

void myLog::asyncWriter() {
    while (true) {
        std::string logLine;
        {
            std::unique_lock<std::mutex> Lock(lock);
            // 阻塞休眠，直到队列有数据，或者收到下班通知
            cv.wait(lock, []() { return logQueue.empty()==false || isRunning==false; });

            if (isRunning==false || logQueue.empty()) {
                return; // 彻底写完，光荣下班

            // 零拷贝转移数据
            logLine = std::move(logQueue.front());
            logQueue.pop();
        }

        // 在无锁状态下写磁盘，绝不卡顿前端业务
        if (fp) {
            fputs(logLine.c_str(), fp);
            fflush(fp);
        }
    }
}