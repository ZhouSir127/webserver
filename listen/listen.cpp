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
#include "../consts.h"

void Listen::acceptNewConnection()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (isListenEt == false)
    {
        int connfd = accept(listenFd, reinterpret_cast<struct sockaddr*>(&client_address), &client_addrlength);
        if (connfd < 0){
            if(errno == EAGAIN || errno == EINTR)
                return;
            LOG_ERROR("accept error: ", errno, " (", strerror(errno), ")");
            return;
        }
        char clientIp[16];
        inet_ntop(AF_INET, &client_address.sin_addr, clientIp, sizeof(clientIp));

        LOG_DEBUG("New connection accepted fd: ", connfd, 
                ", client IP: ", clientIp, 
                ", Port: ", ntohs(client_address.sin_port));

        if (connfd > consts::MAX_FD)
        {
            const std::string info = "Internal server busy";
            send(connfd, info.c_str(), info.length(), 0);
            close(connfd);
            LOG_WARN("Internal server busy. MAX_FD reached. Dropping fd: ", connfd);
            while(accept(listenFd, reinterpret_cast<struct sockaddr*>(&client_address), &client_addrlength)>0 );
        }else{
            timerManager.add(connfd);
            httpManager.add(connfd);
        }
    }else
        while (1)
        {
            int fd = accept(listenFd, reinterpret_cast<struct sockaddr*>(&client_address), &client_addrlength);
            if (fd < 0){
                if(errno == EAGAIN)
                    break;
                else if(errno == EINTR)
                    continue;

                LOG_ERROR("ET mode accept error: ", errno, " (", strerror(errno), ")");
                break;
            }
            
            char clientIp[16];
            inet_ntop(AF_INET, &client_address.sin_addr, clientIp, sizeof(clientIp));

            LOG_DEBUG("New connection accepted (ET). fd: ", fd, 
                    ", client IP: ", clientIp, 
                    ", Port: ", ntohs(client_address.sin_port));
                
            if ( fd > consts::MAX_FD)
            {
                const std::string info = "Internal server busy";
                send(fd, info.c_str(), info.size(), 0);
                close(fd);
                LOG_WARN("Internal server busy (ET). MAX_FD reached. Dropping fd: ", fd);
                while(accept(listenFd, reinterpret_cast<struct sockaddr*>(&client_address), &client_addrlength)>0 );
                break;
            }else{
                timerManager.add(fd);
                httpManager.add(fd);
            }
        }
}
