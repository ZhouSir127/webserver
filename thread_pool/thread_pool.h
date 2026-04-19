#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <queue>
//#include <exception>
#include <thread>
#include <vector>
#include <mutex>
#include <semaphore.h>

#include "../http_conn/http_conn.h"
#include "../timer_manager/timer_manager.h"
#include "../epoll_manager/epoll_manager.h"

#include "../epoll_manager/epoll_manager.h"
#include "../timer_manager/timer_manager.h"

class ThreadPool
{
public:
    ThreadPool(EpollManager&epollManager,TimerManager&timerManager,Http&http,int threadNumber,int maxRequest) 
    :epollManager(epollManager),timerManager(timerManager),http(http),threadNumber(threadNumber),maxRequest(maxRequest){
        if (threadNumber <= 0 || maxRequest <= 0) 
            throw std::invalid_argument("Invalid thread pool parameters");
        
        if (sem_init(&sem, 0, 0) != 0)
            throw std::runtime_error("Semaphore initialization failed");
        
        for (int i = 0; i < threadNumber; ++i) {
        // 直接绑定成员函数，无需 static 转换
            threads.emplace_back(&ThreadPool::run ,this);
            threads.back().detach();
        }
    }
    
    ~ThreadPool(){
        sem_destroy(&sem);
    }

    bool append(int fd, bool isEpollOut);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    void run();

    EpollManager&epollManager;
    TimerManager&timerManager;
    Http&http;

    int threadNumber;        //线程池中的线程数
    int maxRequest;
    std::vector<std::thread> threads;       //描述线程池的数组，其大小为m_thread_number
    std::queue<std::pair<int,bool> > workQueue; //请求队列
    std::mutex queueLocker;       //保护请求队列的互斥锁
    sem_t sem;             //是否有任务需要处理
};

#endif