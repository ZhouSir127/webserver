#include "work_queue.h"

bool WorkQueue::append(int fd, bool isEpollOut)
{
    std::unique_lock<std::mutex> lock(queueLocker);
    
    if (workQueue.size() > maxRequest)
        return false;
    
    workQueue.push({fd,isEpollOut} );
        
    cv.notify_one();

    return true;
}

bool WorkQueue::getWork(std::pair<size_t,bool>& work)
{
    std::unique_lock<std::mutex> lock(queueLocker);
    cv.wait(lock, [this]() -> bool { return !workQueue.empty() || working == false ; } );

    if ( workQueue.empty() && working == false )
        return false;

    work = workQueue.front();
    workQueue.pop();
    return true;
}
