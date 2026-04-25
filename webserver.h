#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include "listen/listen.h"
#include "timer_manager/timer_manager.h"
#include "http_conn/http_conn.h"
#include "thread_pool/thread_pool.h"
#include "args.h"
#include "work_queue/work_queue.h"
#include "death/death.h"
class WebServer
{
public:
WebServer(
        const ListenInfo& listenInfo,
        const TimerInfo& timerInfo,
        const HttpInfo& httpInfo,
        const SqlInfo& sqlInfo,
        const ThreadPoolInfo& threadPoolInfo,
        const LogInfo& logInfo
        );
    
    void eventLoop();
    
private:
    void remove(int fd);
    Death death;
    TimerManager timerManager;
    WorkQueue workQueue;
    HttpManager httpManager;
    ThreadPool threadPool;
    Listen listen;
};
#endif
