#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <thread>
#include <vector>

#include "../http_conn/http_conn.h" 
#include "../timer_manager/timer_manager.h"
#include "../work_queue/work_queue.h"
class ThreadPool
{
public:
    ThreadPool(TimerManager&timerManager,HttpManager&httpManager,int threadNumber,WorkQueue<std::pair<int,bool> >& workQueue) 
    :timerManager(timerManager),httpManager(httpManager),threadNumber(threadNumber),workQueue(workQueue){
        if (threadNumber <= 0 ) 
            throw std::invalid_argument("Invalid thread pool parameters");
            
        for (size_t i = 0; i < threadNumber; ++i)
            threads.emplace_back(&ThreadPool::run ,this);    
    }

    ~ThreadPool() {
        workQueue.stopWork(); // 停止工作队列，通知所有线程退出
    
        for (std::thread &thread : threads)
            if (thread.joinable() )
                thread.join(); // 等待线程结束    
    }
    
private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    void run();

    TimerManager&timerManager;
    HttpManager&httpManager;

    size_t threadNumber;        //线程池中的线程数
    std::vector<std::thread> threads;       //描述线程池的数组，其大小为m_thread_number
    WorkQueue<std::pair<int,bool> >& workQueue;
};

#endif