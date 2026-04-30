#ifndef WORK_QUEUE_H
#define WORK_QUEUE_H

#include <queue>
#include <utility>
#include <mutex>
#include <condition_variable>

template <typename T>
class WorkQueue
{
private:
    std::queue<T> workQueue;
    std::mutex queueLocker;       //保护请求队列的互斥锁
    size_t maxRequest;
    std::condition_variable cv;
    bool working;
public:
    WorkQueue(size_t maxRequest):maxRequest(maxRequest),working(true){}
    bool append(const T&item){
        std::unique_lock<std::mutex> lock(queueLocker);
        
        if (workQueue.size() > maxRequest)
            return false;
        
        workQueue.push(item);
            
        cv.notify_one();

        return true;
    }
    bool getWork(T& work){
        std::unique_lock<std::mutex> lock(queueLocker);
        cv.wait(lock, [this]() -> bool { return workQueue.empty()==false || working == false ; } );

        if ( workQueue.empty() && working == false )
            return false;

        work = std::move(workQueue.front() );
        workQueue.pop();
        return true;
    }
    void stopWork() {
        working = false;
        cv.notify_all(); // 唤醒所有线程，确保它们能退出
    }
};
#endif