#ifndef EPOLL_MANAGER_H
#define EPOLL_MANAGER_H

#include <cstdlib>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>
#include <cerrno>
#include <cstring>
#include "../consts.h"
#include "../channel/channel.h"
#include "../log/log.h"

class EpollManager{

private:
    epoll_event events[consts::MAX_EVENT_NUMBER];
    int epollFd;
    
    static int setNonBlocking(int fd);

    //对文件描述符设置非阻塞
    EpollManager():epollFd(epoll_create1(0) ){
        if (epollFd == -1) {
            LOG_ERROR("epoll_create1 failed! errno: ", errno, " (", strerror(errno), ")");
            exit(EXIT_FAILURE);
        }
    }
    
    ~EpollManager(){
        close(epollFd);
    }

    const EpollManager& operator=(const EpollManager&) = delete;
    EpollManager& operator=(EpollManager&&) = delete;
    EpollManager(const EpollManager&) = delete;
    EpollManager(EpollManager&&) = delete;
public:
    static EpollManager&getInstance(){
        static EpollManager epollManager;
        return epollManager;
    }
    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    bool add(Channel* channel, uint32_t events);
    bool remove(Channel*channel);
    bool modify(Channel*channel, uint32_t events);
    int wait(int timeoutMs = -1);
};

#endif 