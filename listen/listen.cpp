#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <cstring>
#include "listen.h"
#include "../log/log.h"
#include "../epoll_manager/epoll_manager.h"
#include "../consts.h"

bool Listen::acceptNewConnection()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (isListenEt == false)
    {
        int connfd = accept(listenFd, reinterpret_cast<struct sockaddr*>(&client_address), &client_addrlength);
        if (connfd < 0){
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        
        LOG_INFO("New connection accepted, fd: %d", connfd);

        if (connfd > consts::MAX_FD)
        {
            const char *info = "Internal server busy";
            send(connfd, info, strlen(info), 0);
        
            close(connfd);

            LOG_ERROR("%s", "Internal server busy");
            return false;
        }        
        EpollManager::getInstance().add(listenChannel.get() );
        // timerManager.add(connfd);
        // httpManager.add(connfd);
    }
    else
        while (1)
        {
            int fd = accept(listenFd, reinterpret_cast<struct sockaddr*>(&client_address), &client_addrlength);
            if (fd < 0)
                break;
            if ( fd > consts::MAX_FD)
            {
                const char *info = "Internal server busy";
                send(fd, info, strlen(info), 0);
                
                close(fd);

                LOG_ERROR("%s", "Internal server busy");
                return false;
            }
            EpollManager::getInstance().add(listenChannel.get() );
            // timerManager.add(fd);
            // httpManager.add(fd);
        }
    return true;
}
