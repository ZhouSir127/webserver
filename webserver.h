#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <memory>
#include "epoll_manager/epoll_manager.h"
#include "timer_manager/timer_manager.h"
#include "http_conn/http_conn.h"
#include "thread_pool/thread_pool.h"
#include "args.h"

class WebServer
{
public:
WebServer(
        const ServerInfo& serverInfo,
        const EpollInfo& epollInfo,
        const TimerInfo& timerInfo,
        const HttpInfo& httpInfo,
        const SqlInfo& sqlInfo,
        const ThreadPoolInfo& threadPoolInfo,
        const LogInfo& logInfo
        );
    ~WebServer();
    void eventListen();
    void eventLoop();
    
private:
    
    bool acceptNewConnection();

    uint16_t port;
    bool isListenEt;
    int listenFd;
    std::unique_ptr<Channel> listenChannel;

    EpollManager epollManager;
    TimerManager timerManager;
    HttpManager httpManager;
    ThreadPool threadPool;
};
#endif
