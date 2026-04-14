#include "threadpool.h"


bool threadpool::append(int fd, bool EPOLLOUT)
{
    unique_lock<std::mutex> lock(m_queuelocker);
    
    if (m_workqueue.size() > max_requests)
        return false;
    
    m_workqueue.push({fd,EPOLLOUT} );
        
    m_queuestat.post();
    return true;
}

void *threadpool::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

void threadpool::run()
{
    while (true)
    {
        m_queuestat.wait();
    
        m_queuelocker.lock();
    
        int fd=m_workqueue.front().first;
        bool EPOLLOUT = m_workqueue.front().second;
        
        m_workqueue.pop();
        
        m_queuelocker.unlock();
        
        if ( !EPOLLOUT )
            switch(http.process(fd) ){
                case BAD_REQUEST:
                    epoll.removefd(fd);
                    utils.del_timer(fd);
                    http.close_conn(fd);
                    close(fd);
                    break;
                case GET_REQUEST:
                    epoll.modfd(fd,::EPOLLOUT);
                    utils.adjust_timer(fd);
                    break;
                case NO_REQUEST:
                    epoll.modfd(fd,EPOLLIN);
                    utils.adjust_timer(fd);
            }            
        else if (http.write(fd) && http.isLinger(fd) ){
            epoll.modfd(fd,EPOLLIN);
            utils.adjust_timer(fd);
            http.init(fd);
        }else{
            epoll.removefd(fd);
            utils.del_timer(fd);
            http.close_conn(fd);
            close(fd);
        }   
    }
}