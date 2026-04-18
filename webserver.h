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

#include "epoll/epoll.h"
#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"
#include "./timer/lst_timer.h"

class WebServer
{
public:
WebServer(int port,
        bool listenET,bool connectET,  
        char* user, char* passWord, char* databaseName, int sql_num, 
        int thread_num,
        bool close_log
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
    bool connectET;

    threadpool m_pool;
    Log log;

    int m_log_write;
    int m_close_log;
};
#endif
