#include "epoll_manager.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <assert.h>

//对文件描述符设置非阻塞
int EpollManager::setNonBlocking(int fd)
{
    int oldOption = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, oldOption | O_NONBLOCK);
    return oldOption;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
int EpollManager::add(Channel*channel)
{
    epoll_event event;
    event.events = channel -> getEvents();
    event.data.ptr = channel;

    // switch (type)
    //     {
    //     case FdType::LISTEN:
    //         event.events = EPOLLIN;
    //         if (isListenEt)
    //             event.events |= EPOLLET;
    //         break;
    //     case FdType::PIPE:
    //         event.events = EPOLLIN;
    //         break;  
    //     case FdType::CONNECTION:
    //         event.events = EPOLLIN | EPOLLRDHUP | EPOLLONESHOT;
    //         if (isConnectEt)
    //             event.events |= EPOLLET;
    //     default:
    //         break;
    //     }
    setNonBlocking(channel -> getFd() );
    return epoll_ctl(epollFd, EPOLL_CTL_ADD, channel -> getFd(), &event);    
}

int EpollManager::modify(Channel*channel)
{
    epoll_event event;
    event.events = channel -> getEvents();
    return epoll_ctl(epollFd, EPOLL_CTL_MOD, channel -> getFd(), &event);
}

//从内核时间表删除描述符
int EpollManager::remove(int fd)
{
    return epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, 0);
}

bool EpollManager::wait(int timeoutMs)
{
    int num = epoll_wait(epollFd, events, consts::MAX_EVENT_NUMBER, timeoutMs);
    if (num < 0) 
        return false;

    for(int i = 0; i < num; ++i){
        Channel* channel = static_cast<Channel*>(events[i].data.ptr);
        channel -> handleEvent(events[i].events);
    }
        
    return true;
}