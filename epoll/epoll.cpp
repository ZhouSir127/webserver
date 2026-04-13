#include "epoll.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <assert.h>

//对文件描述符设置非阻塞
int Epoll::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, old_option | O_NONBLOCK);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
int Epoll::addfd(int fd,Type type)
{
    epoll_event event;
    event.data.fd = fd;
    
    switch (type)
        {
        case Type::LISTEN:
            event.events = EPOLLIN;
            if (listenET)
                event.events |= EPOLLET;
            break;
        case Type::PIPE:
            event.events = EPOLLIN;
            break;  
        case Type::CONNECTION:
            event.events = EPOLLIN | EPOLLRDHUP | EPOLLONESHOT;
            if (connectET)
                event.events |= EPOLLET;
        }
    
    setnonblocking(fd);
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);    
}

int Epoll::modfd(int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;

    event.events = ev | EPOLLRDHUP | EPOLLONESHOT;
     if (connectET)
        event.events |= EPOLLET;

    return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//从内核时间表删除描述符
int Epoll::removefd(int fd)
{
    return epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
}

int Epoll::wait(int time)
{
    return epoll_wait(epollfd, events, MAX_EVENT_NUMBER, time);
}

