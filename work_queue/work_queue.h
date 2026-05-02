#ifndef WORK_QUEUE_H
#define WORK_QUEUE_H

#include <queue>
#include <utility>
#include <mutex>
#include <condition_variable>
#include "../log/log.h"
#include <climits>

template <typename T>
class WorkQueue
{
private:
    std::queue<T> workQueue;
    std::mutex queueLocker;       //保护请求队列的互斥锁
    size_t maxRequest;
    std::condition_variable cv;
    bool working = true;
    bool isLogQueue;
public:
    WorkQueue():maxRequest(INT_MAX),isLogQueue(false){};
    void init(size_t maxRequest,bool isLogQueue){
        this -> maxRequest = maxRequest;
        this -> isLogQueue = isLogQueue;
    }

    WorkQueue(size_t maxRequest,bool isLogQueue):maxRequest(maxRequest),isLogQueue(isLogQueue){}
    bool append(const T&item){
        if(working == false)
            return false;
        std::unique_lock<std::mutex> lock(queueLocker);

        if (workQueue.size() >= maxRequest){
            if(isLogQueue == false)
                LOG_WARN("Work queue is full! maxRequest: ", maxRequest, ". Task dropped.");
            
            return false;
        }
        workQueue.emplace(item);

        cv.notify_one();

        return true;
    }//主线程 http连接回调和LOG来调用
    bool getWork(T& work){
        std::unique_lock<std::mutex> lock(queueLocker);
        cv.wait(lock, [this]() -> bool { return workQueue.empty()==false || working == false ; } );

        if ( workQueue.empty() && working == false )
            return false;

        work = std::move(workQueue.front() );
        workQueue.pop();
        return true;
    }//后台线程和子线程来调用
    
    void stopWork() {
        if(isLogQueue == false)
            LOG_INFO("Work queue stopped. Remaining tasks: ", workQueue.size(), ". Waking up all waiting threads.");
        working = false;
        cv.notify_all();
    }//myLog::close和threadPool析构来调用
};
#endif