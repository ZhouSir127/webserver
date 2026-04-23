#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <queue>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "../http_conn/http_conn.h"
#include "../timer_manager/timer_manager.h"
#include "../epoll_manager/epoll_manager.h"

#include "../epoll_manager/epoll_manager.h"
#include "../timer_manager/timer_manager.h"
#include "../args.h"

class ThreadPool
{
public:
    ThreadPool(EpollManager&epollManager,TimerManager&timerManager,HttpManager&httpManager,const ThreadPoolInfo& threadPoolInfo) 
    :epollManager(epollManager),timerManager(timerManager),httpManager(httpManager),threadNumber(threadPoolInfo.threadNumer),maxRequest(threadPoolInfo.maxRequest),stop(false){
        if (threadNumber <= 0 || maxRequest <= 0) 
            throw std::invalid_argument("Invalid thread pool parameters");
            
        for (size_t i = 0; i < threadNumber; ++i)
            threads.emplace_back(&ThreadPool::run ,this);
        
    }

    ~ThreadPool() {
        stop = true;
        cv.notify_all(); // 唤醒所有线程，确保它们能退出
        for (std::thread &thread : threads) {
            if (thread.joinable()) {
                thread.join(); // 等待线程结束
            }
        }
    }
    
    bool append(int fd, bool isEpollOut);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    void run();

EpollManager&epollManager;
    TimerManager&timerManager;
    HttpManager&httpManager;

    size_t threadNumber;        //线程池中的线程数
    size_t maxRequest;
    std::vector<std::thread> threads;       //描述线程池的数组，其大小为m_thread_number
    std::queue<std::pair<size_t,bool> > workQueue; //请求队列
    std::mutex queueLocker;       //保护请求队列的互斥锁
    std::condition_variable cv;
    bool stop;
};

#endif