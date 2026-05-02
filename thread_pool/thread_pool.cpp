#include "thread_pool.h"
#include <memory>

void ThreadPool::run()
{
    while (true)
    {
        std::shared_ptr<HttpConn> conn;
        if ( workQueue.getWork(conn) == false )
            return; // 工作队列已停止且没有任务，退出线程

        if (conn.use_count() == 1)
            continue;
        
        if ( conn -> getWrite() == false )
            switch(conn->process() ){//读处理后，无需响应直接关闭
                case HttpCode::CLOSED_CONNECTION:
                    death.add(conn -> getFd() );
                    break;
                case HttpCode::NO_REQUEST://继续读
                    EpollManager::getInstance().modify(conn->getChannel(), EPOLLIN | EPOLLRDHUP | EPOLLONESHOT | (conn ->getConnectEt() ? EPOLLET : static_cast<uint32_t>(0)) );
                    adjustment.add(conn -> getFd() );
                    break;
                case HttpCode::GET_REQUEST://读处理后需要响应
                case HttpCode::BAD_REQUEST:
                case HttpCode::NO_RESOURCE:
                case HttpCode::FORBIDDEN_REQUEST:
                case HttpCode::FILE_REQUEST:
                    EpollManager::getInstance().modify(conn->getChannel(), EPOLLOUT | EPOLLRDHUP | EPOLLONESHOT | (conn ->getConnectEt() ? EPOLLET : static_cast<uint32_t>(0)) );
                    adjustment.add(conn -> getFd() );
                    break;
                default: // 满足 -Wswitch-default，处理预料之外的情况
                    break;
            }            
        else{ 
            HttpCode ret = conn->write();
    
            if ( ret == HttpCode::NO_REQUEST ){//继续写
                EpollManager::getInstance().modify(conn->getChannel(), EPOLLOUT | EPOLLRDHUP | EPOLLONESHOT | (conn ->getConnectEt() ? EPOLLET : static_cast<uint32_t>(0)) );
                adjustment.add(conn -> getFd() );
            }else if ( ret == HttpCode::GET_REQUEST && conn->getLinger() ){
                EpollManager::getInstance() .modify(conn->getChannel(), EPOLLIN | EPOLLRDHUP | EPOLLONESHOT | (conn ->getConnectEt() ? EPOLLET : static_cast<uint32_t>(0)) );
                adjustment.add(conn -> getFd() );
                conn->init();
            }else
                death.add(conn -> getFd() );
        }
    }
}