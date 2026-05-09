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
        
        if ( conn->getRevents() & (EPOLLIN | EPOLLPRI) ){
            if(conn->read() == false )
                death.add(conn -> getFd() );    
            else
                adjustment.add(conn -> getFd() );
        }else if(conn -> getRevents() & EPOLLOUT ){
            if(conn->write() == false )
                death.add(conn -> getFd() );    
            else
                adjustment.add(conn -> getFd() );
        }else if(conn -> getRevents() & (EPOLLERR | EPOLLRDHUP | EPOLLHUP) )
            death.add(conn->getFd() );
    }
}