#ifndef CHANNEL_H
#define CHANNEL_H
// Channel.h (建议作为单独的基础组件)
#include <functional>
#include <sys/epoll.h>


class Channel {
private:
    using EventCallback = std::function<void()>;
    const int fd;
    
    EventCallback readCallback;
    EventCallback writeCallback;
    EventCallback errorCallback;
public:

    Channel(int fd,EventCallback&&readCb, EventCallback&&writeCb, EventCallback&&errorCb)
    : fd(fd), readCallback(std::move(readCb)), writeCallback(std::move(writeCb)), errorCallback(std::move(errorCb) ) {}
    // 核心：当 epoll 触发时，统一调用此函数
    void handleEvent(uint32_t revents);
    int getFd() const { return fd; }
    // ... 其他 getter/setter
};

#endif // CHANNEL_H