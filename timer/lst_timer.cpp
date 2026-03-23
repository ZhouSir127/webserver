#include "lst_timer.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>


bool sort_timer_lst::add_timer(int sock)
{
    if(sock>MAX_FD||users_timer[sock])
        return false;
    
    util_timer *timer = new util_timer(time(nullptr)+LIFE_SPAN,sock);

    util_timer*prev=tail->prev;

    tail->prev=timer;
    timer->next=tail;

    prev->next=timer; 
    timer->prev=prev;

    users_timer[sock]=timer;
    return true;
}

bool sort_timer_lst::adjust_timer(int sockfd)
{
    if(sockfd>MAX_FD||users_timer[sockfd]==nullptr)
        return false;
    
    util_timer *timer = users_timer[sockfd];
    
    if( timer!=tail->prev ){
        util_timer *prev=timer->prev,*next=timer->next;
        
        next->prev=prev;
        prev->next=next;

        prev=tail->prev;

        prev->next=timer; 
        timer->prev=prev;

        tail->prev=timer;
        timer->next=tail;
    }
    
    timer->expire=time(nullptr)+LIFE_SPAN;
    return true;
}

bool sort_timer_lst::del_timer(int sockfd)
{
    if(sockfd>MAX_FD||users_timer[sockfd]==nullptr)
        return false;
    
    util_timer *prev=users_timer[sockfd]->prev,*next=users_timer[sockfd]->next;

    prev->next=next;
    next->prev=prev;
    
    delete users_timer[sockfd];
    users_timer[sockfd]=nullptr;

    return true;
}

void sort_timer_lst::tick()
{
    util_timer *end =head->next;
    time_t now=time(nullptr);
    //(head,end)
    while(end!=tail&&end->expire<=now)
        end=end->next;

    death.clear();
    
    for ( util_timer *tmp=head->next,*next=tmp->next; tmp!=end; tmp=next,next=tmp->next ){
        death.push_back(tmp->sockfd);
        users_timer[tmp->sockfd]=nullptr;
        delete tmp;
    }
    
    end->prev=head;
    head->next=end;
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



