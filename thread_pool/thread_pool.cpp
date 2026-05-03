#include "thread_pool.h"
#include <memory>

void ThreadPool::run()
{
    while (true)
    {
        std::shared_ptr<HttpConn> conn;
        if ( workQueue.getWork(conn) == false )
            return; 

        if (conn.use_count() == 1)
            continue;
        
        if ( conn -> getChannel()->getRevents() & (EPOLLIN | EPOLLPRI) ){
            switch(conn -> read() ){
                case HttpCode::CLOSED_CONNECTION:
                case HttpCode::BAD_REQUEST:
                    death.add(conn -> getFd() );
                    break;    
                case HttpCode::GET_REQUEST:   
                    EpollManager::getInstance().modify(conn->getChannel(), EPOLLOUT | EPOLLRDHUP | EPOLLONESHOT | (conn ->getConnectEt() ? EPOLLET : static_cast<uint32_t>(0)) );
                    adjustment.add(conn -> getFd() );
                    break;
                default:
                    break;
            }
        }else if(conn -> getChannel()->getRevents() & EPOLLOUT ){
            HttpCode ret = HttpCode::GET_REQUEST;
            bool dead(false);

            do{

            if(){
                ret = conn->process();
                if( ret == HttpCode::CLOSED_CONNECTION){
                    death.add(conn -> getFd() );
                    dead = true;
                    break;
                }else if ( ret == HttpCode::NO_REQUEST){
                    EpollManager::getInstance().modify(conn->getChannel(), EPOLLIN | EPOLLRDHUP | EPOLLONESHOT | (conn ->getConnectEt() ? EPOLLET : static_cast<uint32_t>(0)) );
                    adjustment.add(conn -> getFd() );
                    break;
                }    // GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,
            }
                ret = conn->write();
                
                if ( ret == HttpCode::NO_REQUEST ){
                    EpollManager::getInstance().modify(conn->getChannel(), EPOLLOUT | EPOLLRDHUP | EPOLLONESHOT | (conn ->getConnectEt() ? EPOLLET : static_cast<uint32_t>(0)) );
                    adjustment.add(conn -> getFd() );
                    break;
                }else if ( !(ret == HttpCode::GET_REQUEST && conn->getLinger() ) ){
                    death.add(conn -> getFd() );
                    dead = true;
                    break;
                }
            }while( parseEnd()==false ||  sendEnd() == false );
            if(ret != HttpCode::NO_REQUEST && dead == false ){
                conn->init();
                EpollManager::getInstance().modify(conn->getChannel(), EPOLLIN | EPOLLRDHUP | EPOLLONESHOT | (conn ->getConnectEt() ? EPOLLET : static_cast<uint32_t>(0)) );
                adjustment.add(conn -> getFd() );
            }
        }else
            death.add(conn->getFd() );
    }
}