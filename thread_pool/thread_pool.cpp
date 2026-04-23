#include "thread_pool.h"

bool ThreadPool::append(int fd, bool isEpollOut)
{
    std::unique_lock<std::mutex> lock(queueLocker);
    
    if (workQueue.size() > maxRequest)
        return false;
    
    workQueue.push({fd,isEpollOut} );
        
    cv.notify_one();

    return true;
}

void ThreadPool::run()
{
    while (true)
    {
        int fd;
        bool isEpollOut;
        {
        std::unique_lock<std::mutex> lock(queueLocker);
        cv.wait(lock, [this]() -> bool { return !workQueue.empty() || stop; });

        if (stop && workQueue.empty())
            return;

        fd = workQueue.front().first;
        isEpollOut = workQueue.front().second;
        
        workQueue.pop();
        }

        if ( !isEpollOut )
            switch(httpManager.process(fd) ){//读处理后，无需响应直接关
                case HttpCode::CLOSED_CONNECTION:
                    epollManager.remove(fd);
                    timerManager.remove(fd);
                    httpManager.remove(fd);
                    close(fd);
                    break;
                case HttpCode::NO_REQUEST://继续读
                    epollManager.modify(fd,EPOLLIN);
                    timerManager.adjust(fd);
                    break;
                case HttpCode::GET_REQUEST://读处理后需要响应
                case HttpCode::BAD_REQUEST:
                case HttpCode::NO_RESOURCE:
                case HttpCode::FORBIDDEN_REQUEST:
                case HttpCode::FILE_REQUEST:
                    epollManager.modify(fd,::EPOLLOUT);
                    timerManager.adjust(fd);
                    break;
                default: // 满足 -Wswitch-default，处理预料之外的情况
                    break;
            }            
        else{ 
            HttpCode ret = httpManager.write(fd);
    
            if ( ret == HttpCode::NO_REQUEST ){//继续写
                epollManager.modify(fd,EPOLLOUT);
                timerManager.adjust(fd);
            }else if ( ret == HttpCode::GET_REQUEST && httpManager.getLinger(fd) ){
                epollManager.modify(fd,EPOLLIN);
                timerManager.adjust(fd);
                httpManager.init(fd);
            }else{
                epollManager.remove(fd);
                timerManager.remove(fd);
                httpManager.remove(fd);
                close(fd);
            }
        }
    }
}