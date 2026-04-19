#include "thread_pool.h"

bool ThreadPool::append(int fd, bool isEpollOut)
{
    std::unique_lock<std::mutex> lock(queueLocker);
    
    if (workQueue.size() > maxRequest)
        return false;
    
    workQueue.push({fd,isEpollOut} );
        
    return sem_post(&sem)==0;
}

void ThreadPool::run()
{
    while (true)
    {
        sem_wait(&sem);
        int fd;
        bool isEpollOut;

        {
        std::unique_lock<std::mutex> lock(queueLocker);
        if (workQueue.empty()) continue;
        
        fd = workQueue.front().first;
        isEpollOut = workQueue.front().second;
        
        workQueue.pop();
        }


        if ( !isEpollOut )
            switch(http.process(fd) ){//读处理后，无需响应直接关
                case HttpCode::CLOSED_CONNECTION:
                    epollManager.remove(fd);
                    timerManager.remove(fd);
                    http.remove(fd);
                    close(fd);
                    break;
                case HttpCode::NO_REQUEST://继续读
                    epollManager.modify(fd,EPOLLIN);
                    timerManager.adjust(fd);
                default:    //准备发送响应
                    epollManager.modify(fd,::EPOLLOUT);
                    timerManager.adjust(fd);
                    break;
            }            
        else{
            HttpCode ret = http.write(fd) ;
    
            if ( ret == HttpCode::NO_REQUEST ){//继续写
                epollManager.modify(fd,EPOLLOUT);
                timerManager.adjust(fd);
            }else if ( ret == HttpCode::GET_REQUEST && http.getLinger(fd) ){
                epollManager.modify(fd,EPOLLIN);
                timerManager.adjust(fd);
                http.init(fd);
            }else{
                epollManager.remove(fd);
                timerManager.remove(fd);
                http.remove(fd);
                close(fd);
            }
        }
    }
}