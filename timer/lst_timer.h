#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"
#include "../consts.h"

#include <vector>
#include <memory>

class sort_timer_lst
{
private:

    struct util_timer{    
        util_timer()=default;
        
        util_timer(time_t expire,int sock):expire(expire),sockfd(sock){}
        
        time_t expire;
        int sockfd;
        
        std::weak_ptr<util_timer>prev;
        std::weak_ptr<util_timer>next;
    };

    int lifeSpan;

    std::shared_ptr<util_timer> head;
    std::shared_ptr<util_timer> tail;
    std::vector<std::shared_ptr<util_timer> > users_timer;    

    std::vector<int> death;

public:
    sort_timer_lst(int lifeSpan):lifeSpan(lifeSpan),head(std::make_shared<util_timer>()),tail(std::make_shared<util_timer>()),users_timer(consts::MAX_FD+1){
        head->next=tail;
        tail->prev=head;
    }
    
    void add_timer(int sock);
    void adjust_timer(int sockfd);
    void del_timer(int sockfd);
    void tick();
    const std::vector<int>& getDeath() const { return death; }
    
    bool exist(int fd)const {        
        return (bool)users_timer[fd];
    }
};


class Signal{

public:
    Signal() = delete;

    static int getPipefd1() { return m_pipefd[1]; }   
    static int getPipefd0() { return m_pipefd[0]; }

    static void alarm()  { ::alarm(timeSlot); }
    static bool dealwithsignal (bool &timeout, bool &stop_server);

    static void init(int slot){
        timeSlot = slot;
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
        assert(ret != -1);

        addsig(SIGPIPE, SIG_IGN);
        addsig(SIGALRM, sig_handler, false);
        addsig(SIGTERM, sig_handler, false);

        alarm();
    }
    static void clean() {
        close(m_pipefd[1]);
        close(m_pipefd[0]);
    }

private:
    static int timeSlot;
    
    //设置信号函数
    static void addsig(int sig, void(*handler)(int), bool restart = true);
    //信号处理函数
    static void sig_handler(int sig);

    static int m_pipefd[2];
};

class Utils{

public:
    Utils(int lifeSpan,int timeSlot):m_timer_lst(lifeSpan){
        Signal::init(timeSlot);
    }
    ~Utils(){
        Signal::clean();
    }
        //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();    
    int getPipefd1() const { return Signal::getPipefd1(); }
    int getPipefd0() const { return Signal::getPipefd0(); }
    void add_timer(int sock) { m_timer_lst.add_timer(sock); }
    void adjust_timer(int sockfd) { m_timer_lst.adjust_timer(sockfd); }
    void del_timer(int sockfd) { m_timer_lst.del_timer(sockfd); }
    bool dealwithsignal(bool &timeout, bool &stop_server) { return Signal::dealwithsignal(timeout, stop_server); }
    const std::vector<int>& getDeath() const { return m_timer_lst.getDeath(); }

private:
    sort_timer_lst m_timer_lst;
};

#endif
