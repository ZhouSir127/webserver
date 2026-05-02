#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <thread>
#include <vector>

#include "../work_queue/work_queue.h"
#include "../set/set.h"
#include "../http_conn/http_conn.h"
#include "../timer_manager/timer_manager.h"

class ThreadPool
{
public:
    ThreadPool(int threadNumber,WorkQueue<std::shared_ptr<HttpConn> >& workQueue,Set&death,Set&adjustment) 
    :threadNumber(threadNumber),workQueue(workQueue),death(death),adjustment(adjustment){
        if (threadNumber <= 0 ) 
            throw std::invalid_argument("Invalid thread pool parameters");
            
        for (size_t i = 0; i < threadNumber; ++i)
            threads.emplace_back(&ThreadPool::run,this);    
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

    size_t threadNumber;        //线程池中的线程数
    std::vector<std::thread> threads;       //描述线程池的数组，其大小为m_thread_number
    WorkQueue<std::shared_ptr<HttpConn> >& workQueue;
    Set&death;
    Set&adjustment;
};

#endif