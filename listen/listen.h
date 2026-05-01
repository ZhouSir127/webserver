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
#include "../timer_manager/timer_manager.h"
#include "../http_conn/http_conn.h"

class Listen
{
private:
    uint16_t port;
    bool isListenEt;
    int listenFd;
    std::unique_ptr<Channel> listenChannel;
    TimerManager& timerManager;
    HttpManager& httpManager;
    
    bool acceptNewConnection();

public:
    Listen(const ListenInfo& listenInfo, TimerManager& timerManager, HttpManager& httpManager)
    :port(listenInfo.port),isListenEt(listenInfo.isListenEt),listenFd(socket(PF_INET, SOCK_STREAM, 0)),
    listenChannel(std::make_unique<Channel>(
        listenFd,
        [this](){ acceptNewConnection(); },
        nullptr,
        nullptr
    )), 
    timerManager(timerManager),httpManager(httpManager)
    {
        if (listenFd < 0){
            LOG_ERROR("Create listen socket failed: ", errno, " (", strerror(errno), ")");
            exit(EXIT_FAILURE);
        }
        int flag = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

        //绑定地址结构
        struct sockaddr_in address;
        bzero(&address, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);

        int ret = bind(listenFd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
        if(ret < 0){
            LOG_ERROR("Server bind port ", port, " failed: ", errno, " (", strerror(errno), ")");
            exit(EXIT_FAILURE);
        }
        ret = listen(listenFd, listenInfo.backlog);
        if (ret < 0){ 
            LOG_ERROR("Server listen port ", port, " failed: ", errno, " (", strerror(errno), ")");
            exit(EXIT_FAILURE);
        }

        EpollManager::getInstance().add(listenChannel.get(),isListenEt ? (EPOLLIN | EPOLLET) : EPOLLIN );    
        LOG_INFO("WebServer started successfully, listening on port ", port);
    }
    
    ~Listen(){
        EpollManager::getInstance().remove(listenChannel.get() );
        close(listenFd);
    }
};

#endif
