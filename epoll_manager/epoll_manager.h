#ifndef EPOLL_MANAGER_H
#define EPOLL_MANAGER_H

#include <cstdlib>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>
#include "../consts.h"
#include "../args.h"
#include "../channel/channel.h"

class EpollManager{

private:
    epoll_event events[consts::MAX_EVENT_NUMBER];
    int epollFd;
    bool isListenEt;//true:ET ,false:LT
    bool isConnectEt;    

    static int setNonBlocking(int fd);

public:
    
    //对文件描述符设置非阻塞
    EpollManager(const EpollInfo& epollInfo):epollFd(epoll_create1(0)),isListenEt(epollInfo.isListenEt),isConnectEt(epollInfo.isConnectEt){
        if (epollFd == -1) 
            exit(EXIT_FAILURE);
    }
    
    ~EpollManager(){
        close(epollFd);
    }
    
    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    int add(Channel* channel);
    int remove(int fd);
    int modify(Channel* channel);
    bool wait(int timeoutMs = -1);
};

#endif 