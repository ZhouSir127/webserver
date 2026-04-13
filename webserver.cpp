#include "webserver.h"

WebServer::WebServer(int port,
                bool listenET,bool connectET,  
                char* user, char* passWord, char* databaseName, int sql_num, 
                int thread_num,
                int log_write,int close_log
                )
:m_port(port), 
listenET(listenET),connectET(connectET),
epoll(listenET,connectET),
http(connectET,user, passWord, databaseName,sql_num),
m_pool ( new threadpool(epoll,utils,http,thread_num) ),
m_log_write(log_write),m_close_log(close_log)
{
     if (0 == m_close_log)
    {
        //初始化日志
        if (1 == m_log_write)
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        else
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
}

WebServer::~WebServer()
{
    close(m_listenfd);
    delete m_pool;
}

void WebServer::eventListen()
{
    //网络编程基础步骤
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);
    
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    //优雅关闭连接
    struct linger tmp = {1, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    //绑定地址结构
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);
}

bool WebServer::dealclientdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (listenET == false)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (connfd > MAX_FD)
        {
            const char *info = "Internal server busy";
            send(connfd, info, strlen(info), 0);
        
            close(connfd);

            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        
        epoll.addfd(connfd,Epoll::Type::CONNECTION);
        utils.add_timer(connfd);
        http.add_http(connfd);
    }
    else
        while (1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
                break;
            if ( connfd > MAX_FD)
            {
                const char *info = "Internal server busy";
                send(connfd, info, strlen(info), 0);
                
                close(connfd);

                LOG_ERROR("%s", "Internal server busy");
                return false;
            }
            epoll.addfd(connfd,Epoll::Type::CONNECTION);
            utils.add_timer(connfd);
            http.add_http(connfd);
        }
    return true;
}

void WebServer::eventLoop()
{
        //epoll对象创建内核事件表，并注册监听文件描述符
    epoll.addfd(m_listenfd,Epoll::Type::LISTEN);
    epoll.addfd(utils.getPipefd0(), Epoll::Type::PIPE);

    bool timeout = false;
    bool stop_server = false;
    
    do{
        int number = epoll.wait();
        if (number < 0 ){
            if(errno != EINTR)
            {
                LOG_ERROR("%s", "epoll failure");
                break;
            }
            continue;
        }

        for (int i = 0; i < number ; i++){
            const epoll_event&event=epoll.getEvent(i);

            if (event.data.fd == m_listenfd){
                if (event.events & EPOLLIN)
                    dealclientdata();
                else
                    LOG_ERROR("%s", "epoll failure");
            }else if ( event.data.fd == utils.getPipefd0() ){
                if(event.events & EPOLLIN ){
                    if(false == utils.dealwithsignal(timeout,stop_server) )
                        LOG_ERROR("%s", "dealclientdata failure");
                }else
                    LOG_ERROR("%s", "epoll failure");
            }else if(event.events & EPOLLIN)
                m_pool->append(event.data.fd, false);
            else if(event.events && EPOLLOUT)
                m_pool->append(event.data.fd, true);
            else{    
                epoll.removefd(event.data.fd);
                utils.del_timer(event.data.fd);
                http.close_conn(event.data.fd);
                close(event.data.fd);
            }
        }
        
        if (timeout){
            utils.timer_handler();
            
            for(int fd : utils.getDeath() ){
                epoll.removefd(fd);
                http.close_conn(fd);
                close(fd);

                LOG_INFO("close fd %d", fd);
            }

            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }while (stop_server==false);
}
