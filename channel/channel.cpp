#include "channel.h"
#include "../log/log.h"
void Channel::handleEvent(uint32_t revents) {
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
