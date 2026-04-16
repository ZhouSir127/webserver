#include "threadpool.h"


bool threadpool::append(int fd, bool EPOLLOUT)
{
    unique_lock<std::mutex> lock(m_queuelocker);
    
    if (m_workqueue.size() > max_requests)
        return false;
    
    m_workqueue.push({fd,EPOLLOUT} );
        
    return sem_post(&m_sem)==0;
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
        sem_wait(&m_sem);
        int fd;
        bool EPOLLOUT;

        {
        unique_lock<std::mutex> lock(m_queuelocker);
    
        fd=m_workqueue.front().first;
        EPOLLOUT = m_workqueue.front().second;
        
        m_workqueue.pop();
        }


        if ( !EPOLLOUT )
            switch(http.process(fd) ){
                case CLOSED_CONNECTION:
                    epoll.removefd(fd);
                    utils.del_timer(fd);
                    http.close_conn(fd);
                    close(fd);
                    break;
                case NO_REQUEST:
                    epoll.modfd(fd,EPOLLIN);
                    utils.adjust_timer(fd);
                default:
                    epoll.modfd(fd,::EPOLLOUT);
                    utils.adjust_timer(fd);
                    break;
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