#include "timer_manager.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <cstring>
#include "../log/log.h"
#include <algorithm>

bool TimerMinHeap::keep(size_t idx){
    size_t leftIdx((idx<<1)+1);
    
    if (leftIdx >= size ) 
        return false;

    time_t leftExpire = fdToExpireIdx[heap[leftIdx]].first;
    time_t expire = fdToExpireIdx[heap[idx] ].first;
    size_t rightIdx(leftIdx + 1);

    if(rightIdx >= size){
        if(leftExpire < expire ){
            std::swap(heap[idx], heap[leftIdx]);

            fdToExpireIdx[heap[idx]].second = idx;
            fdToExpireIdx[heap[leftIdx]].second = leftIdx;
            keep(leftIdx);
            return true;
        }
    }else{
        time_t rightExpire = fdToExpireIdx[heap[rightIdx]].first;

        if ( leftExpire < expire || rightExpire < expire )
        {
            size_t swapIdx = leftExpire < rightExpire ? leftIdx : rightIdx;

            std::swap(heap[idx], heap[swapIdx]);

            fdToExpireIdx[heap[idx]].second = idx;
            fdToExpireIdx[heap[swapIdx]].second = swapIdx;
            keep(swapIdx);
            return true;
        }
    }
    return false;
}

void TimerMinHeap::pop(){
    fdToExpireIdx[heap[0]] = {-1,-1};
    
    if(--size > 0){
        heap[0] = heap[size];
        fdToExpireIdx[heap[0]].second = 0;    
        keep(0);
    }
}

void TimerMinHeap::add(int fd)
{
    std::unique_lock<std::mutex>Lock(lock);
    fdToExpireIdx[fd] = {time(nullptr)+lifeSpan, size};
    heap[size++] = fd;
}

void TimerMinHeap::adjust(int fd){
    std::unique_lock<std::mutex>Lock(lock);

    fdToExpireIdx[fd].first = time(nullptr)+lifeSpan;
    
    keep(fdToExpireIdx[fd].second);    
}

void TimerMinHeap::remove(int fd)
{    
    std::unique_lock<std::mutex>Lock(lock);

    if(fdToExpireIdx[fd].second == size-1){
        fdToExpireIdx[fd] = {-1,-1};
        --size;
        return;
    }

    size_t idx = fdToExpireIdx[fd].second;
    heap [idx] = heap[size - 1];
    fdToExpireIdx[heap[idx]].second = idx;
    --size;
    
    fdToExpireIdx[fd] = {-1,-1};
    
    if (!keep(idx) )
    for(int parent = (idx-1)>>1; parent >= 0; idx = parent, parent = (parent-1)>>1){
        if(fdToExpireIdx[heap[parent] ].first > fdToExpireIdx[heap[idx] ].first ){
            std::swap(heap[parent], heap[idx]);
            fdToExpireIdx[heap[parent]].second = parent;
            fdToExpireIdx[heap[idx]].second = idx;
        }else 
            break;
    }    
}

void TimerMinHeap::tick()
{
    std::unique_lock<std::mutex>Lock(lock);

    time_t now=time(nullptr);
    
    death.clear();
    while (size > 0)
    {
        int fd = heap[0];
        if (fdToExpireIdx[fd].first > now)
            break;
        
        death.push_back(fd);
        pop();
    }
}

unsigned int SignalHandler::timeSlot;
int SignalHandler::pipefd[2];

//信号处理函数
void SignalHandler::sigHandler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    
    if(write(pipefd[1], reinterpret_cast<char*>(&sig), 1) < 0){
        LOG_ERROR("Failed to write signal to pipe: %s", strerror(errno));
    }
    
    errno = save_errno;
    
}

//设置信号函数
void SignalHandler::addSig(int sig, void(*handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    
    sa.sa_handler = handler;
    
    if (restart)
        sa.sa_flags |= SA_RESTART;
    
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

void SignalHandler::dealWithSignal (bool &timeout, bool &stopServer) 
{
    char signals[1024];
    ssize_t ret = recv(pipefd[0], signals, sizeof(signals), 0);
    if (ret <= 0)
        return;
    
    for (int i = 0; i < ret; ++i)
        if (signals[i] == SIGALRM )
            timeout = true;
        else if (signals[i] == SIGTERM )
            stopServer = true;   
}




