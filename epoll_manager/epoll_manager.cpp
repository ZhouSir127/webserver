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
int EpollManager::add(Channel*channel, uint32_t events)
{
    epoll_event event;
    event.events = events;
    event.data.ptr = channel;
    setNonBlocking(channel -> getFd() );
    return epoll_ctl(epollFd, EPOLL_CTL_ADD, channel -> getFd(), &event);    
}

int EpollManager::modify(int fd, uint32_t events)
{
    epoll_event event;
    event.events = events;
    return epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &event);
}

//从内核时间表删除描述符
int EpollManager::remove(int fd)
{
    return epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, 0);
}

int EpollManager::wait(int timeoutMs)
{
    int num = epoll_wait(epollFd, events, consts::MAX_EVENT_NUMBER, timeoutMs);
    if (num < 0) 
        return -1;

    for(int i = 0; i < num; ++i){
        Channel* channel = static_cast<Channel*>(events[i].data.ptr);
        channel -> handleEvent(events[i].events);
    }

    return num;
}