#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <string>

#include "epoll/epoll.h"
#include "threadpool/threadpool.h"
#include "http/http_conn.h"
#include "timer_manager/timer_manager.h"

class WebServer
{
public:
WebServer(int port,
        bool listenET,bool connectET,
        timer_t lifeSpan,time_t timeSlot,
        const std::string&IP,int sqlport,const std::string& user, const std::string& passWord, const std::string& databaseName, int sql_num, const std::string&root,
        int thread_num,int max_request,
        const std::string& file_name,bool close_log
        );
    ~WebServer();
    void eventListen();
    void eventLoop();

private:
    
    bool dealclientdata();

    Epoll epoll;
    Utils utils;
    HTTP http;
    
    int m_port;
    int m_listenfd;

    bool listenET;
    
    threadpool m_pool;
};
#endif
