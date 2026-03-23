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
void Epoll::addfd(int fd,Type type)
{
    epoll_event event;
    event.data.fd = fd;
    setnonblocking(fd);

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
            break;  
        default:
            break;
        }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);    
}

//将事件重置为EPOLLONESHOT
void Epoll::modfd(int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;

    event.events = ev | EPOLLRDHUP | EPOLLONESHOT;

     if (connectET)
        event.events |= EPOLLET;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//从内核时间表删除描述符
void Epoll::removefd(int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

int Epoll::wait(int timeout)
{
    return epoll_wait(epollfd, events, MAX_EVENT_NUMBER, timeout);
}

bool Epoll::isListenEvent(int idx,int m_listenfd) const{
    return events[idx].data.fd == m_listenfd && (events[idx].events & EPOLLIN);   
}


bool Epoll::isSignal(int idx,int pipeFd0) const{
    return events[idx].data.fd == pipeFd0 && (events[idx].events & EPOLLIN);
}

int Epoll::isConnectionEvent(int idx) const{   
    if(events[idx].events & EPOLLIN )
        return events[idx].data.fd;
    else if(events[idx].events & EPOLLOUT)
        return -events[idx].data.fd;
    
    return 0;
}

int Epoll::isErrorEvent(int idx) const{
    if(events[idx].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR) )
        return events[idx].data.fd;

    return 0;
}