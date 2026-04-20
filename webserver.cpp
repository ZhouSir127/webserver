#include "webserver.h"
#include "epoll_manager/epoll_manager.h"

WebServer::WebServer(int port,
                bool listenET,bool connectET,  
                int lifeSpan,int timeSlot,
                const std::string&IP,int sqlport,const std::string& user, const std::string& passWord, const std::string& databaseName, int sql_num,const std::string&root,
                int thread_num,int max_request,
                const std::string& file_name,bool close_log 
            )
:port(port),
isListenEt(listenET),
epollManager(listenET,connectET),
timerManager(lifeSpan,timeSlot),
http(connectET, IP, sqlport,user, passWord, databaseName,sql_num,root),
threadPool(epollManager,timerManager,http,thread_num,max_request)
{
    Log::init(file_name,close_log);
}

WebServer::~WebServer()
{
    close(listenFd);
}

void WebServer::eventListen()
{
    //网络编程基础步骤
    listenFd = socket(PF_INET, SOCK_STREAM, 0);

    assert(listenFd >= 0);
    
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

    int ret = bind(listenFd, (struct sockaddr *)&address, sizeof(address));
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

bool WebServer::dealClientData()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (isListenEt == false)
    {
        int connfd = accept(listenFd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
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
        
        epollManager.add(connfd,EpollManager::FdType::CONNECTION);
        timerManager.add(connfd);
        http.add(connfd);
    }
    else
        while (1)
        {
            int connfd = accept(listenFd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
                break;
            if ( connfd > consts::MAX_FD)
            {
                const char *info = "Internal server busy";
                send(connfd, info, strlen(info), 0);
                
                close(connfd);

                LOG_ERROR("%s", "Internal server busy");
                return false;
            }
            epollManager.add(connfd,EpollManager::FdType::CONNECTION);
            timerManager.add(connfd);
            http.add(connfd);
        }
    return true;
}

void WebServer::eventLoop()
{
    epollManager.add(listenFd,EpollManager::FdType::LISTEN);
    epollManager.add(timerManager.getPipeReadFd(), EpollManager::FdType::PIPE);

    bool timeout = false;
    bool stopServer = false;
    
    do{
        int number = epollManager.wait();
        if (number < 0 ){
            if(errno != EINTR)
            {
                LOG_ERROR("%s", "epoll failure");
                break;
            }
            continue;
        }

        for (int i = 0; i < number ; i++){
            const epoll_event&event=epollManager.getEvent(i);

            if (event.data.fd == listenFd){
                if (event.events & EPOLLIN)
                    dealClientData();
                else
                    LOG_ERROR("%s", "epoll failure");
            }else if ( event.data.fd == timerManager.getPipeReadFd() ){
                if(event.events & EPOLLIN )
                    timerManager.dealWithSignal(timeout,stopServer);
                else
                    LOG_ERROR("%s", "epoll failure");
            }else if(event.events & EPOLLIN)
                threadPool.append(event.data.fd, false);
            else if(event.events & EPOLLOUT)
                threadPool.append(event.data.fd, true);
            else{    
                epollManager.remove(event.data.fd);
                timerManager.remove(event.data.fd);
                http.remove(event.data.fd);
                close(event.data.fd);
            }
        }
        
        if (timeout){
            timerManager.timerHandler();
            
            for(int fd : timerManager.getDeath() ){
                epollManager.remove(fd);
                http.remove(fd);
                close(fd);

                LOG_INFO("close fd %d", fd);
            }

            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }while (stopServer==false);
}
