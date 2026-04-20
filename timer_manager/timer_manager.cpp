#include "timer_manager.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <cstring>
#include "../log/log.h"

void SortedTimerList::add(int fd)
{
    std::unique_lock<std::mutex>Lock(lock);

    std::shared_ptr<Node> node = std::make_shared<Node> (time(nullptr)+lifeSpan,fd);

    node->next=tail;
    node->prev=tail->prev;

    std::shared_ptr<Node> next = node -> next.lock();
    std::shared_ptr<Node> prev = node -> prev.lock();

    next->prev=node;
    prev->next=node;
        
    fdToNode[fd]=node;
}

void SortedTimerList::adjust(int fd){
    std::unique_lock<std::mutex>Lock(lock);

    std::shared_ptr<Node>&timer = fdToNode[fd];
    timer->expire=time(nullptr)+lifeSpan;

    if( timer != tail->prev.lock() ){    
    
        std::shared_ptr<Node> next = timer -> next.lock();
        std::shared_ptr<Node> prev = timer -> prev.lock();

        next->prev=prev;
        prev->next=next;
        
        timer->next=tail;
        timer->prev=tail->prev;
        
        std::shared_ptr<Node> Next = timer -> next.lock();
        std::shared_ptr<Node> Prev = timer -> prev.lock();

        Next->prev=timer;
        Prev->next=timer;
    }
    
}

void SortedTimerList::remove(int fd)
{    
    std::unique_lock<std::mutex>Lock(lock);

    std::shared_ptr<Node> &timer = fdToNode[fd];

    std::shared_ptr<Node> next = timer -> next.lock();
    std::shared_ptr<Node> prev = timer -> prev.lock();

    prev->next=next;
    next->prev=prev;
    
    timer.reset();
}

void SortedTimerList::tick()
{
    std::unique_lock<std::mutex>Lock(lock);

    std::shared_ptr<Node> end = head -> next.lock();
    time_t now=time(nullptr);
    
    death.clear();

    while( end!=tail && end->expire <= now){
        death.push_back(end->fd);
        fdToNode[end->fd].reset();
        end=end->next.lock();
        LOG_INFO("Connection timeout, fd: %d. Marking for removal.", end->fd);
    }

    end->prev = head;
    head->next = end;
}

unsigned int SignalHandler::timeSlot;
int SignalHandler::pipefd[2];

//信号处理函数
void SignalHandler::sigHandler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    write(pipefd[1], reinterpret_cast<char*>(&sig), 1);
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




