#ifndef WORK_QUEUE_H
#define WORK_QUEUE_H

#include <queue>
#include <utility>
#include <mutex>
#include <condition_variable>

class WorkQueue
{
private:
    std::queue<std::pair<size_t,bool> > workQueue;
    std::mutex queueLocker;       //保护请求队列的互斥锁
    size_t maxRequest;
    std::condition_variable cv;
    bool working;
public:
    WorkQueue(size_t maxRequest):maxRequest(maxRequest),working(true){}
    bool append(int fd, bool isEpollOut);
    bool getWork(std::pair<size_t,bool>& work);
    void stopWork() {
        working = false;
        cv.notify_all(); // 唤醒所有线程，确保它们能退出
    }
};
#endif