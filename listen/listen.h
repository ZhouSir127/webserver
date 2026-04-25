#ifndef LISTEN_H 
#define LISTEN_H
#include <sys/socket.h>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <strings.h> 
#include "../args.h"
#include "../epoll_manager/epoll_manager.h"
#include "../args.h"
#include "../channel/channel.h"

class Listen
{
private:
    uint16_t port;
    bool isListenEt;
    int listenFd;
    std::unique_ptr<Channel> listenChannel;
    bool acceptNewConnection();

public:
    Listen(const ListenInfo& listenInfo)
    :port(listenInfo.port),isListenEt(listenInfo.isListenEt),listenFd(socket(PF_INET, SOCK_STREAM, 0)),listenChannel(std::make_unique<Channel>(
        listenFd,
        isListenEt ? (EPOLLIN | EPOLLET) : EPOLLIN,
        [this](){ acceptNewConnection(); },
        nullptr,
        nullptr
    )) {
            EpollManager::getInstance().add(listenChannel.get() );

            int flag = 1;
            setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

            //优雅关闭连接
            struct linger tmp = {1, 1};
            setsockopt(listenFd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

            //绑定地址结构
            struct sockaddr_in address;
            bzero(&address, sizeof(address));
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_ANY);
            address.sin_port = htons(port);

            int ret = bind(listenFd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
            if(ret < 0){
                LOG_ERROR("Server bind port %d failed", port);
                exit(4399);
            }
            ret = listen(listenFd, 5);
            if (ret < 0){ 
                LOG_ERROR("Server listen port %d failed", port);
                exit(4399);
            }
            LOG_INFO("WebServer started successfully, listening on port %d", port);
    }
    
    ~Listen() {
        close(listenFd);
    }
    void eventListen();


};

#endif
