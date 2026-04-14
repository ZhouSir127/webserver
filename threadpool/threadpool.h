#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <vector>
//#include "../lock/locker.h"
#include <mutex>
#include <condition_variable>

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
    std::condition_variable m_queuestat;            //是否有任务需要处理
};



#endif