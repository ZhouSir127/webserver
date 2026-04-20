#ifndef EPOLL_MANAGER_H
#define EPOLL_MANAGER_H

#include <cstdlib>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>
#include "../consts.h"

class EpollManager{

private:
    epoll_event events[consts::MAX_EVENT_NUMBER];
    int epollFd;
    bool isListenEt;//true:ET ,false:LT
    bool isConnectEt;    

    static int setNonBlocking(int fd);

public:
    enum class FdType :unsigned char{
        LISTEN,
        PIPE,
        CONNECTION
    };
    
    //对文件描述符设置非阻塞
    
    EpollManager(bool listenET,bool connectET):epollFd(epoll_create1(0)),isListenEt(listenET),isConnectEt(connectET){
        if (epollFd == -1) {
            exit(EXIT_FAILURE);
        }
    }
    
    ~EpollManager(){
        close(epollFd);
    }
    
    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    int add(int fd,FdType type);
    int remove(int fd);
    int modify(int fd, int ev);
    int wait(int timeoutMs = -1);

    const epoll_event& getEvent(int idx)const{
        return events[idx];
    }
};

#endif 