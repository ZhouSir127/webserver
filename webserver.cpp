#include "webserver.h"
#include "epoll_manager/epoll_manager.h"
#include "work_queue/work_queue.h"

WebServer::WebServer(
                const ListenInfo& listenInfo,  
                const TimerInfo& timerInfo,
                const HttpInfo& httpInfo,
                const SqlInfo& sqlInfo,
                const ThreadPoolInfo& threadPoolInfo,
                const LogInfo& logInfo  
            )
:timerManager(timerInfo, death),
workQueue(threadPoolInfo.maxRequest),
httpManager(httpInfo,sqlInfo,workQueue,death),
threadPool(timerManager,httpManager,threadPoolInfo.threadNumer,workQueue),
listen(listenInfo, timerManager, httpManager)
{
    Log::init(logInfo.file,logInfo.close);
}

void WebServer::remove(int fd){
    timerManager.remove(fd);
    httpManager.remove(fd);
    close(fd);
}

void WebServer::eventLoop()
{
    do{
        int number = EpollManager::getInstance().wait();
        if (number < 0 ){
            if(errno != EINTR){
                LOG_ERROR("%s", "epoll failure");
                break;
            }    
        }
        if (timerManager.getTimeout() ){
            timerManager.timerHandler();
            LOG_INFO("%s", "timer tick");
        }
        for(int fd : death.getDeath() ){
            remove(fd);
            LOG_INFO("close fd %d", fd);
        }
            
    }while (timerManager.getStopServer() == false);
}
