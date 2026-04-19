#include "timer_manager.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <cstring>
#include "../log/log.h"

void SortedTimerList::add(int sock)
{
    std::unique_lock<std::mutex>Lock(lock);

    std::shared_ptr<TimerNode> timer = std::make_shared<TimerNode> (time(nullptr)+lifeSpan,sock);

    timer->next=tail;
    timer->prev=tail->prev;

    std::shared_ptr<TimerNode> next = timer -> next.lock();
    std::shared_ptr<TimerNode> prev = timer -> prev.lock();

    next->prev=timer;
    prev->next=timer;
        
    usersTimer[sock]=timer;
}

void SortedTimerList::adjust(int sock){
    std::unique_lock<std::mutex>Lock(lock);

    std::shared_ptr<TimerNode>&timer = usersTimer[sock];
    timer->expire=time(nullptr)+lifeSpan;

    if( timer != tail->prev.lock() ){    
    
        std::shared_ptr<TimerNode> next = timer -> next.lock();
        std::shared_ptr<TimerNode> prev = timer -> prev.lock();

        next->prev=prev;
        prev->next=next;
        
        timer->next=tail;
        timer->prev=tail->prev;
        
        std::shared_ptr<TimerNode> Next = timer -> next.lock();
        std::shared_ptr<TimerNode> Prev = timer -> prev.lock();

        Next->prev=timer;
        Prev->next=timer;
    }
    
}

void SortedTimerList::remove(int sock)
{    
    std::unique_lock<std::mutex>Lock(lock);

    std::shared_ptr<TimerNode> &timer = usersTimer[sock];

    std::shared_ptr<TimerNode> next = timer -> next.lock();
    std::shared_ptr<TimerNode> prev = timer -> prev.lock();

    prev->next=next;
    next->prev=prev;
    
    timer.reset();
}

void SortedTimerList::tick()
{
    std::unique_lock<std::mutex>Lock(lock);

    std::shared_ptr<TimerNode> end = head -> next.lock();
    time_t now=time(nullptr);
    
    death.clear();

    while( end!=tail && end->expire <= now){
        death.push_back(end->sock);
        usersTimer[end->sock].reset();
        end=end->next.lock();
        LOG_INFO("Connection timeout, fd: %d. Marking for removal.", end->sock);
    }

    end->prev = head;
    head->next = end;
}

time_t SignalHandler::timeSlot;
int SignalHandler::pipefd[2];

//信号处理函数
void SignalHandler::sigHandler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    write(pipefd[1], (char *)&sig, 1);
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
    assert(sigaction(sig, &sa, NULL) != -1);
}

bool SignalHandler::dealWithSignal (bool &timeout, bool &stopServer) 
{
    char signals[1024];
    int ret = recv(pipefd[0], signals, sizeof(signals), 0);
    if (ret <= 0)
        return false;
    
    for (int i = 0; i < ret; ++i)
        switch (signals[i]){
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stopServer = true;
                break;
            }
        }
    
    return true;
}




