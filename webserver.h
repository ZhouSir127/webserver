#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <string>

#include "epoll_manager/epoll_manager.h"
#include "timer_manager/timer_manager.h"
#include "http_conn/http_conn.h"
#include "thread_pool/thread_pool.h"

class WebServer
{
public:
WebServer(int port,
        bool listenET,bool connectET,
        int lifeSpan,int timeSlot,
        const std::string&IP,int sqlport,const std::string& user, const std::string& passWord, const std::string& databaseName, int sql_num, const std::string&root,
        int thread_num,int max_request,
        const std::string& file_name,bool close_log
        );
    ~WebServer();
    void eventListen();
    void eventLoop();

private:
    
    bool dealClientData();

    int port;
    bool isListenEt;
    int listenFd;

    EpollManager epollManager;
    TimerManager timerManager;
    Http http;

    
    
    ThreadPool threadPool;
};
#endif
