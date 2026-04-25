#include "thread_pool.h"

void ThreadPool::run()
{
    while (true)
    {
        std::pair<size_t,bool> work;
        if ( !workQueue.getWork(work) )
            return; // 工作队列已停止且没有任务，退出线程

        if ( !work.second )
            switch(httpManager.process(work.first) ){//读处理后，无需响应直接关闭
                case HttpCode::CLOSED_CONNECTION:
            //        EpollManager::getInstance().remove(fd);
                    timerManager.remove(work.first);
                    httpManager.remove(work.first);
                    close(work.first);
                    break;
                case HttpCode::NO_REQUEST://继续读
                    //EpollManager::getInstance().modify(fd,EPOLLIN);
                    timerManager.adjust(work.first);
                    break;
                case HttpCode::GET_REQUEST://读处理后需要响应
                case HttpCode::BAD_REQUEST:
                case HttpCode::NO_RESOURCE:
                case HttpCode::FORBIDDEN_REQUEST:
                case HttpCode::FILE_REQUEST:
                    //EpollManager::getInstance().modify(fd,::EPOLLOUT);
                    timerManager.adjust(work.first);
                    break;
                default: // 满足 -Wswitch-default，处理预料之外的情况
                    break;
            }            
        else{ 
            HttpCode ret = httpManager.write(work.first);
    
            if ( ret == HttpCode::NO_REQUEST ){//继续写
        //        EpollManager::getInstance().modify(fd,EPOLLOUT);
                timerManager.adjust(work.first);
            }else if ( ret == HttpCode::GET_REQUEST && httpManager.getLinger(work.first) ){
        //        EpollManager::getInstance() .modify(fd,EPOLLIN);
                timerManager.adjust(work.first);
                httpManager.init(work.first);
            }else{
        //        EpollManager::getInstance().remove(fd);
                timerManager.remove(work.first);
                httpManager.remove(work.first);
                close(work.first);
            }
        }
    }
}