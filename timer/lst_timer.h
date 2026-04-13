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
        
        util_timer(time_t time,int sock):expire(time),sockfd(sock){}
        
        time_t expire;
        int sockfd;
        
        std::weak_ptr<util_timer>prev;
        std::weak_ptr<util_timer>next;
    };

    std::shared_ptr<util_timer> head;
    std::shared_ptr<util_timer> tail;
    std::vector<std::shared_ptr<util_timer> > users_timer;    

    std::vector<int> death;

public:
    sort_timer_lst():head(std::make_shared<util_timer>()),tail(std::make_shared<util_timer>()),users_timer(MAX_FD+1){
        head->next=tail;
        tail->prev=head;
    }
    
    bool add_timer(int sock);
    bool adjust_timer(int sockfd);
    bool del_timer(int sockfd);
    void tick();
    const std::vector<int>& getDeath() const { return death; }
    
    bool exist(int fd)const {        
        if(fd>MAX_FD)
            return false;
        return users_timer[fd]!=nullptr;
    }
};


class Signal{

public:
    static Signal& getInstance(){
        static Signal signal;
        return signal;
    }

    int getPipefd1() const { return m_pipefd[1]; }   
    int getPipefd0() const { return m_pipefd[0]; }

    void alarm() const { ::alarm(TIMESLOT); }
    bool dealwithsignal (bool &timeout, bool &stop_server) const ;

private:

    Signal(){
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
        assert(ret != -1);

        addsig(SIGPIPE, SIG_IGN);
        addsig(SIGALRM, sig_handler, false);
        addsig(SIGTERM, sig_handler, false);

        alarm();
    }
    
    ~Signal() {
        close(m_pipefd[1]);
        close(m_pipefd[0]);
    }

    Signal(const Signal&) = delete;    // 禁止拷贝构造
    Signal& operator=(const Signal&) = delete;  // 禁止赋值
    // 禁止移动构造和移动赋值
    Signal(Signal&&) = delete;
    Signal& operator=(Signal&&) = delete;

    //设置信号函数
    void addsig(int sig, void(*handler)(int), bool restart = true);
    //信号处理函数
    static void sig_handler(int sig);

    static int m_pipefd[2];
};

class Utils{

public:
        //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();
    
    int getPipefd1() const { return Signal::getInstance().getPipefd1(); }
    int getPipefd0() const { return Signal::getInstance().getPipefd0(); }
    bool add_timer(int sock) { return m_timer_lst.add_timer(sock); }
    bool adjust_timer(int sockfd) { return m_timer_lst.adjust_timer(sockfd); }
    bool del_timer(int sockfd) { return m_timer_lst.del_timer(sockfd); }
    void tick() { m_timer_lst.tick(); }
    bool dealwithsignal(bool &timeout, bool &stop_server) { return Signal::getInstance().dealwithsignal(timeout, stop_server); }
    const std::vector<int>& getDeath() const { return m_timer_lst.getDeath(); }

private:
    sort_timer_lst m_timer_lst;
};



#endif
