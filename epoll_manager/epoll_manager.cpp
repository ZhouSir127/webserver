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
bool EpollManager::add(Channel*channel, uint32_t events)
{
    epoll_event event;
    event.events = events;
    event.data.ptr = channel;
    setNonBlocking(channel -> getFd() );
    int ret = epoll_ctl(epollFd, EPOLL_CTL_ADD, channel -> getFd(), &event);    
    if(ret < 0){
        LOG_ERROR("epoll_ctl ADD failed for fd: ", channel->getFd(), 
                  ". errno: ", errno, " (", strerror(errno), ")");
        return false;
    }
    return true;
}

bool EpollManager::modify(Channel*channel, uint32_t events)
{
    epoll_event event;
    event.events = events;
    event.data.ptr = channel;

    int ret = epoll_ctl(epollFd, EPOLL_CTL_MOD, channel -> getFd(), &event);

    if (ret < 0) {
        LOG_ERROR("epoll_ctl MOD failed for fd: ", channel->getFd(), 
                  ". errno: ", errno, " (", strerror(errno), ")");
        return false;
    }
    return true;
}

//从内核时间表删除描述符
bool EpollManager::remove(Channel*channel)
{
    int ret = epoll_ctl(epollFd, EPOLL_CTL_DEL, channel -> getFd(), 0);

    if (ret < 0 && errno != ENOENT) {
        // ENOENT 表示要删的 fd 本来就不在 epoll 里，这种情况在断开连接时很常见，忽略即可。
        // 只有发生其他系统错误时才记录 ERROR
        LOG_ERROR("epoll_ctl DEL failed for fd: ", channel->getFd(), 
                  ". errno: ", errno, " (", strerror(errno), ")");
        return false;    
    }
    return true;
}

int EpollManager::wait(int timeoutMs)
{
    int num = epoll_wait(epollFd, events, consts::MAX_EVENT_NUMBER, timeoutMs);
    
    for(int i = 0; i < num; ++i){
        Channel* channel = static_cast<Channel*>(events[i].data.ptr);
        channel -> handleEvent(events[i].events);
    }

    return num;
}