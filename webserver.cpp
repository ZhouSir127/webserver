#include "webserver.h"
#include "epoll_manager/epoll_manager.h"
#include "work_queue/work_queue.h"

WebServer::WebServer(
                const TimerInfo& timerInfo,
                const HttpInfo& httpInfo,const SqlInfo& sqlInfo,const RedisInfo& redisInfo,
                const ThreadPoolInfo& threadPoolInfo,
                const ListenInfo& listenInfo,  
                const LogInfo& logInfo  
            )
:timerManager(timerInfo, death),
workQueue(threadPoolInfo.maxRequest,false),
httpManager(httpInfo,sqlInfo,redisInfo,workQueue,death),
threadPool(timerManager,httpManager,threadPoolInfo.threadNumer,workQueue),
listen(listenInfo,timerManager,httpManager)
{
    myLog::init(logInfo);
    LOG_INFO("========== WebServer initialized and starting ==========");
}

WebServer::~WebServer()
{
    LOG_INFO("========== WebServer is shutting down ==========");
    myLog::close();
}

void WebServer::eventLoop()
{
    do{
        int number = EpollManager::getInstance().wait();
        if (number < 0 ){
            if(errno != EINTR){
                LOG_ERROR("epoll wait failure! errno: ", errno, " (", strerror(errno), ")");
                break;
            }    
        }
        if (timerManager.getTimeout() ){
            timerManager.timerHandler();
            LOG_DEBUG("timer tick handled");
        }
        for(int fd : death.getDeath() ){
            timerManager.remove(fd);
            httpManager.remove(fd);
            LOG_DEBUG("close fd: ", fd);
        }
            
    }while (timerManager.getStopServer() == false);
}
