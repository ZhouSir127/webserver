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

    time_t leftExpire = fdToNode[heap[leftIdx]].expire;
    time_t expire = fdToNode[heap[idx] ].expire;
    size_t rightIdx(leftIdx + 1);

    if(rightIdx >= size){
        if(leftExpire < expire ){
            std::swap(heap[idx], heap[leftIdx]);

            fdToNode[heap[idx]].idx = idx;
            fdToNode[heap[leftIdx]].idx = leftIdx;
            keep(leftIdx);
            return true;
        }
    }else{
        time_t rightExpire = fdToNode[heap[rightIdx]].expire;

        if ( leftExpire < expire || rightExpire < expire )
        {
            size_t swapIdx = leftExpire < rightExpire ? leftIdx : rightIdx;

            std::swap(heap[idx], heap[swapIdx]);

            fdToNode[heap[idx]].idx = idx;
            fdToNode[heap[swapIdx]].idx = swapIdx;
            keep(swapIdx);
            return true;
        }
    }
    return false;
}

void TimerMinHeap::pop(){
    fdToNode[heap[0]] = {-1,-1};
    
    if(--size > 0){
        heap[0] = heap[size];
        fdToNode[heap[0]].idx = 0;    
        keep(0);
    }
}

void TimerMinHeap::add(int fd)
{
    fdToNode[fd] = {time(nullptr)+lifeSpan, size};
    heap[size++] = fd;
}

void TimerMinHeap::adjust(int fd){
    if(fdToNode[fd].idx == -1)
        return;

    fdToNode[fd].expire = time(nullptr)+lifeSpan;
    
    keep(fdToNode[fd].idx);    
}

void TimerMinHeap::remove(int fd)
{    
    if (fdToNode[fd].idx == -1)
        return; 
    
    if(fdToNode[fd].idx == size-1){
        fdToNode[fd] = {-1,-1};
        --size;
        return;
    }

    size_t idx = fdToNode[fd].idx;
    heap [idx] = heap[size - 1];
    fdToNode[heap[idx]].idx = idx;
    --size;
    
    fdToNode[fd] = {-1,-1};
    
    if (!keep(idx) )
    while(idx > 0){
        int parent = (idx-1) >> 1;
        if(fdToNode[heap[parent] ].expire > fdToNode[heap[idx] ].expire ){
            std::swap(heap[parent], heap[idx]);
            fdToNode[heap[parent]].idx = parent;
            fdToNode[heap[idx]].idx = idx;
        }else 
            break;
        idx = parent;
    }
}

void TimerMinHeap::tick()
{

    time_t now=time(nullptr);
    
    while (size > 0)
    {
        int fd = heap[0];
        if (fdToNode[fd].expire > now)
            break;
    
        death.add(fd);
        pop();
    }
}

unsigned int SignalHandler::timeSlot;
int SignalHandler::pipefd[2];
bool SignalHandler::timeout;
bool SignalHandler::stopServer;
std::unique_ptr<Channel> SignalHandler::signalChannel;

//信号处理函数
void SignalHandler::sigHandler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    
    int ret = write(pipefd[1], reinterpret_cast<char*>(&sig), 1);
    if(ret < 0)
        return;

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

void SignalHandler::dealWithSignal () 
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




