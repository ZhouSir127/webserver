#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
//#include <exception>
#include <pthread.h>
#include <vector>
#include <mutex>
#include <semaphore.h>

#include "../CGImysql/sql_connection_pool.h"
#include "../http/http_conn.h"
#include "../timer/lst_timer.h"
#include "../epoll/epoll.h"
#include "../consts.h"

class threadpool
{
public:
    threadpool(Epoll&epoll,Utils&utils,HTTP&http,int thread_number) 
    :epoll(epoll),utils(utils),http(http),m_thread_number(thread_number), m_threads(m_thread_number){
        for (int i = 0; i < thread_number; ++i)
        {
            if (pthread_create(&m_threads[i], nullptr , worker, this) != 0)
                throw std::exception();
            
            if (pthread_detach(m_threads[i]) )
                throw std::exception();
        }

        if (sem_init(&m_sem, 0, 0) != 0)
            throw std::exception();
    }
    
    ~threadpool(){
        sem_destroy(&m_sem);
    }

    bool append(int fd, bool EPOLLOUT);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

    Epoll&epoll;
    Utils&utils;
    HTTP&http;

    int m_thread_number;        //线程池中的线程数
    std::vector<pthread_t> m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::queue<std::pair<int,bool> > m_workqueue; //请求队列
    std::mutex m_queuelocker;       //保护请求队列的互斥锁
    sem_t m_sem;             //是否有任务需要处理
};

#endif