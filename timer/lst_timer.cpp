#include "lst_timer.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>


bool sort_timer_lst::add_timer(int sock)
{
    if(sock>MAX_FD||users_timer[sock] )
        return false;
    
    std::shared_ptr<util_timer> timer = std::make_shared<util_timer> (time(nullptr)+LIFE_SPAN,sock);

    timer->next=tail;
    timer->prev=tail->prev;

    std::shared_ptr<util_timer> next = timer -> next.lock();
    std::shared_ptr<util_timer> prev = timer -> prev.lock();

    next->prev=timer;
    prev->next=timer;
        
    users_timer[sock]=timer;
    return true;
}

bool sort_timer_lst::adjust_timer(int sockfd){
    if(sockfd>MAX_FD||users_timer[sockfd]==nullptr )
        return false;
    
    std::shared_ptr<util_timer>&timer = users_timer[sockfd];
    timer->expire=time(nullptr)+LIFE_SPAN;

    if( timer != tail->prev.lock() ){    
    
        std::shared_ptr<util_timer> next = timer -> next.lock();
        std::shared_ptr<util_timer> prev = timer -> prev.lock();

        next->prev=prev;
        prev->next=next;
        
        timer->next=tail;
        timer->prev=tail->prev;
        
        std::shared_ptr<util_timer> Next = timer -> next.lock();
        std::shared_ptr<util_timer> Prev = timer -> prev.lock();

        Next->prev=timer;
        Prev->next=timer;
    }
    
    return true;
}

bool sort_timer_lst::del_timer(int sockfd)
{
    if(sockfd>MAX_FD||users_timer[sockfd]==nullptr )
        return false;
    
    std::shared_ptr<util_timer> &timer = users_timer[sockfd];

    std::shared_ptr<util_timer> next = timer -> next.lock();
    std::shared_ptr<util_timer> prev = timer -> prev.lock();

    prev->next=next;
    next->prev=prev;
    
    timer.reset();

    return true;
}

void sort_timer_lst::tick()
{
    std::shared_ptr<util_timer> end = head -> next.lock();
    time_t now=time(nullptr);
    
    death.clear();

    while( end!=tail && end->expire <= now){
        death.push_back(end->sockfd);
        users_timer[end->sockfd].reset();
        end=end->next.lock();
    }

    end->prev = head;
    head->next = end;
}

int Signal::m_pipefd[2];

//信号处理函数
void Signal::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    write(m_pipefd[1], (char *)&msg, 1);
    errno = save_errno;
}

//设置信号函数
void Signal::addsig(int sig, void(*handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

bool Signal::dealwithsignal (bool &timeout, bool &stop_server) const
{
    int sig;
    char signals[1024];
    int ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
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
                stop_server = true;
                break;
            }
        }
    
    return true;
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    Signal::getInstance().alarm();
    m_timer_lst.tick();
}



