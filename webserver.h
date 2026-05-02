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
        const TimerInfo& timerInfo,
        const HttpInfo& httpInfo,const SqlInfo& sqlInfo,const RedisInfo& redisInfo,
        const ThreadPoolInfo& threadPoolInfo,
        const ListenInfo& listenInfo,  
        const LogInfo& logInfo
        );
    
    ~WebServer();

    void eventLoop();

private:    
    Death death;
    TimerManager timerManager;
    WorkQueue<std::pair<int,bool> > workQueue;
    HttpManager httpManager;
    ThreadPool threadPool;
    Listen listen;

};
#endif
