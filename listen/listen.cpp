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

bool Listen::acceptNewConnection()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (isListenEt == false)
    {
        int connfd = accept(listenFd, reinterpret_cast<struct sockaddr*>(&client_address), &client_addrlength);
        if (connfd < 0){
            LOG_ERROR("accept error: ", errno, " (", strerror(errno), ")");
            return false;
        }
        
        LOG_DEBUG("New connection accepted (ET). fd: ", connfd, 
                      ", client IP: ", inet_ntoa(client_address.sin_addr), 
                      ", Port: ", ntohs(client_address.sin_port));

        if (connfd > consts::MAX_FD)
        {
            const char *info = "Internal server busy";
            send(connfd, info, strlen(info), 0);
        
            close(connfd);

            LOG_ERROR("Internal server busy. MAX_FD reached. Dropping fd: ", connfd);
            return false;
        }        
        timerManager.add(connfd);
        httpManager.add(connfd);
    }
    else
        while (1)
        {
            int fd = accept(listenFd, reinterpret_cast<struct sockaddr*>(&client_address), &client_addrlength);
            if (fd < 0){
                if(errno == EAGAIN)
                    break;
                LOG_ERROR("ET mode accept error: ", errno, " (", strerror(errno), ")");
                return false;
            }
            
            LOG_DEBUG("New connection accepted (ET). fd: ", fd, 
                      ", client IP: ", inet_ntoa(client_address.sin_addr), 
                      ", Port: ", ntohs(client_address.sin_port));
            
            if ( fd > consts::MAX_FD)
            {
                std::string info = "Internal server busy";
                send(fd, info.c_str(), info.size(), 0);
                
                close(fd);

                LOG_ERROR("Internal server busy (ET). MAX_FD reached. Dropping fd: ", fd);

                return false;
            }
            timerManager.add(fd);
            httpManager.add(fd);
        }
    return true;
}
