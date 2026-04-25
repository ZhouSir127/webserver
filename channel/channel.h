#ifndef CHANNEL_H
#define CHANNEL_H
// Channel.h (建议作为单独的基础组件)
#include <functional>
#include <sys/epoll.h>
#include "../log/log.h"

class Channel {
private:
    using EventCallback = std::function<void()>;
    const int fd;
    uint32_t events;   // 用户关心的事件 (如 EPOLLIN)

    EventCallback readCallback;
    EventCallback writeCallback;
    EventCallback errorCallback;
public:

    Channel(int fd,uint32_t events,EventCallback&&readCb, EventCallback&&writeCb, EventCallback&&errorCb)
    : fd(fd), events(events), readCallback(std::move(readCb)), writeCallback(std::move(writeCb)), errorCallback(std::move(errorCb) ) {}
    // 核心：当 epoll 触发时，统一调用此函数
    void handleEvent(uint32_t revents) {
        // 根据 epoll 传回的具体事件(revents_)，调用对应的回调
        if( revents & EPOLLIN ){
            if(readCallback)
                readCallback();
            else
                LOG_ERROR("%s", "epoll failure");
        }
        if( revents & EPOLLOUT ){
            if(writeCallback)
                writeCallback();
            else
                LOG_ERROR("%s", "epoll failure");
        }
        if(revents & (EPOLLERR | EPOLLRDHUP | EPOLLHUP) ){
            if (errorCallback)
                errorCallback();
            else 
                LOG_ERROR("%s", "epoll failure");
        }
        // 还可以增加错误处理 EPOLLERR 等
    }
    uint32_t getEvents() const { return events; }
    int getFd() const { return fd; }
    // ... 其他 getter/setter
};

#endif // CHANNEL_H