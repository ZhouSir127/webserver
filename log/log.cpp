#include "log.h"
# include <stdarg.h>
#include <climits>
#include "../work_queue/work_queue.h"


namespace myLog{

std::vector<std::string> levelVec{"[debug]: ","[info]: ","[warn]: ","[erro]: "};
FILE* fp = nullptr;
bool closeLog;
std::thread bgThread;
WorkQueue<std::string> workQueue;

void init(const LogInfo&logInfo) {
    
    if ( (closeLog = logInfo.close) )
        return; // 如果配置了关闭日志，直接返回，不打开文件
    
    fp = fopen(logInfo.file.c_str(), "a");
    bgThread = std::thread(asyncWriter);
    workQueue.init(logInfo.maxRequest,true);
}

void append(const std::string&str) {
    workQueue.append(str);
}

void close() {
    workQueue.stopWork();

    if (bgThread.joinable() )
        bgThread.join(); // 阻塞主线程，直到后台线程把队列里最后一点日志写完

    if (fp != nullptr) {
        fclose(fp);
        fp = nullptr;
    }
}

void asyncWriter() {
    while (true) {
        std::string logLine;
        if( workQueue.getWork(logLine) ==false )
            return;
        
        // 在无锁状态下写磁盘，绝不卡顿前端业务
        if (fp) {
            fputs(logLine.c_str(), fp);
            fflush(fp);
        }
    }
}

}
