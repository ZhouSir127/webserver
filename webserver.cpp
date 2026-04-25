#include "webserver.h"
#include "epoll_manager/epoll_manager.h"

WebServer::WebServer(
                const ListenInfo& listenInfo,  
                const TimerInfo& timerInfo,
                const HttpInfo& httpInfo,
                const SqlInfo& sqlInfo,
                const ThreadPoolInfo& threadPoolInfo,
                const LogInfo& logInfo
            )
:listen(listenInfo),
timerManager(timerInfo),
httpManager(httpInfo,sqlInfo),
threadPool(timerManager,httpManager,threadPoolInfo)
{
    Log::init(logInfo.file,logInfo.close);
}

void WebServer::eventLoop()
{
    bool timeout = false;
    bool stopServer = false;
    
    do{
        int number = EpollManager::getInstance().wait();
        if (number < 0 ){
            if(errno != EINTR)
            {
                LOG_ERROR("%s", "epoll failure");
                break;
            }
            continue;
        }

            // if (event.data.fd == listenFd){
            //     if (event.events & EPOLLIN)
            //         acceptNewConnection();//读，错误事件
            // }else if ( event.data.fd == timerManager.getPipeReadFd() ){
            //     if(event.events & EPOLLIN )
            //         timerManager.dealWithSignal(timeout,stopServer);
            //     else
            //         LOG_ERROR("%s", "epoll failure");
            // }//读，错误
        //连接套接字：读，写，错误事件
        //     else if(event.events & EPOLLIN)
        //         threadPool.append(event.data.fd, false);
        //     else if(event.events & EPOLLOUT)
        //         threadPool.append(event.data.fd, true);
        //     else{    
        //         epollManager.remove(event.data.fd);
        //         timerManager.remove(event.data.fd);
        //         httpManager.remove(event.data.fd);
        //         close(event.data.fd);
        //     }
        // }
        
        if (timeout){
            timerManager.timerHandler();
            
            for(int fd : timerManager.getDeath() ){
                EpollManager::getInstance().remove(fd);
                httpManager.remove(fd);
                close(fd);

                LOG_INFO("close fd %d", fd);
            }

            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }while (stopServer==false);
}
