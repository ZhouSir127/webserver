#include "thread_pool.h"
#include <memory>

void ThreadPool::run()
{
    while (true)
    {
        std::pair<int,bool> work;
        if ( !workQueue.getWork(work) )
            return; // 工作队列已停止且没有任务，退出线程

        std::shared_ptr<HttpConn> conn = httpManager.getSharedConn(work.first);
        if (!conn)
            continue;
        
        if ( !work.second )
            switch(conn->process() ){//读处理后，无需响应直接关闭
                case HttpCode::CLOSED_CONNECTION:
                    death.add(work.first);
                    break;
                case HttpCode::NO_REQUEST://继续读
                    EpollManager::getInstance().modify(conn->getChannel(), EPOLLIN | EPOLLRDHUP | EPOLLONESHOT | (httpManager.getConnectEt() ? EPOLLET : static_cast<uint32_t>(0)) );
                    timerManager.adjust(work.first);
                    break;
                case HttpCode::GET_REQUEST://读处理后需要响应
                case HttpCode::BAD_REQUEST:
                case HttpCode::NO_RESOURCE:
                case HttpCode::FORBIDDEN_REQUEST:
                case HttpCode::FILE_REQUEST:
                    EpollManager::getInstance().modify(conn->getChannel(), ::EPOLLOUT | EPOLLRDHUP | EPOLLONESHOT | (httpManager.getConnectEt() ? EPOLLET : static_cast<uint32_t>(0)) );
                    timerManager.adjust(work.first);
                    break;
                default: // 满足 -Wswitch-default，处理预料之外的情况
                    break;
            }            
        else{ 
            HttpCode ret = conn->write();
    
            if ( ret == HttpCode::NO_REQUEST ){//继续写
                EpollManager::getInstance().modify(conn->getChannel(), EPOLLOUT | EPOLLRDHUP | EPOLLONESHOT | (httpManager.getConnectEt() ? EPOLLET : static_cast<uint32_t>(0)) );
                timerManager.adjust(work.first);
            }else if ( ret == HttpCode::GET_REQUEST && conn->getLinger() ){
                EpollManager::getInstance() .modify(conn->getChannel(), EPOLLIN | EPOLLRDHUP | EPOLLONESHOT | (httpManager.getConnectEt() ? EPOLLET : static_cast<uint32_t>(0)) );
                timerManager.adjust(work.first);
                conn->init();
            }else
                death.add(work.first);
        }
    }
}