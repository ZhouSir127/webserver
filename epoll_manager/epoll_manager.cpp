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
int EpollManager::add(int fd,FdType type)
{
    epoll_event event;
    event.data.fd = fd;
    
    switch (type)
        {
        case FdType::LISTEN:
            event.events = EPOLLIN;
            if (isListenEt)
                event.events |= EPOLLET;
            break;
        case FdType::PIPE:
            event.events = EPOLLIN;
            break;  
        case FdType::CONNECTION:
            event.events = EPOLLIN | EPOLLRDHUP | EPOLLONESHOT;
            if (isConnectEt)
                event.events |= EPOLLET;
        }
    
    setNonBlocking(fd);
    return epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);    
}

int EpollManager::modify(int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;

    event.events = ev | EPOLLRDHUP | EPOLLONESHOT;
     if (isConnectEt)
        event.events |= EPOLLET;

    return epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &event);
}

//从内核时间表删除描述符
int EpollManager::remove(int fd)
{
    return epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, 0);
}

int EpollManager::wait(int timeoutMs)
{
    return epoll_wait(epollFd, events, consts::MAX_EVENT_NUMBER, timeoutMs);
}

