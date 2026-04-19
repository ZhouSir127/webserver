#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <pthread.h>

#include <sys/mman.h>
#include <stdarg.h>

#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../consts.h"

#include <vector>
#include <memory>
#include <mutex>

class SortedTimerList
{
private:

    struct TimerNode{    
        TimerNode()=default;
        
        TimerNode(time_t expire,int sock):expire(expire),sock(sock){}
        
        time_t expire;
        int sock;
        
        std::weak_ptr<TimerNode>prev;
        std::weak_ptr<TimerNode>next;
    };

    std::mutex lock;
    time_t lifeSpan;
    std::shared_ptr<TimerNode> head;
    std::shared_ptr<TimerNode> tail;
    std::vector<std::shared_ptr<TimerNode> > usersTimer;    

    std::vector<int> death;

public:
    SortedTimerList(time_t lifeSpan):lifeSpan(lifeSpan),head(std::make_shared<TimerNode>()),tail(std::make_shared<TimerNode>()),usersTimer(consts::MAX_FD+1){
        head->next=tail;
        tail->prev=head;
    }
    
    void add(int sock);
    void adjust(int sock);
    void remove(int sock);
    void tick();
    const std::vector<int>& getDeath() const { return death; }
    
    bool exist(int fd)const {        
        return (bool)usersTimer[fd];
    }
};


class SignalHandler{

public:
    SignalHandler() = delete;

    static int getPipefd1() { return pipefd[1]; }   
    static int getPipefd0() { return pipefd[0]; }

    static void setAlarm()  { alarm(timeSlot); }
    static bool dealWithSignal (bool &timeout, bool &stopServer);

    static void init(int slot){
        timeSlot = slot;
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
        assert(ret != -1);

        addSig(SIGPIPE, SIG_IGN);
        addSig(SIGALRM, sigHandler, false);
        addSig(SIGTERM, sigHandler, false);

        setAlarm();
    }
    static void clean() {
        close(pipefd[1]);
        close(pipefd[0]);
    }

private:
    static time_t timeSlot;
    static int pipefd[2];
    //设置信号函数
    static void addSig(int sig, void(*handler)(int), bool restart = true);
    //信号处理函数
    static void sigHandler(int sig);
};

class TimerManager{

public:
    TimerManager(int lifeSpan,int timeSlot):timerList(lifeSpan){
        SignalHandler::init(timeSlot);
    }
    ~TimerManager(){
        SignalHandler::clean();
    }
        //定时处理任务，重新定时以不断触发SIGALRM信号
    void timerHandler(){
        timerList.tick();
        SignalHandler::setAlarm();
    }
    int getPipefd1() const { return SignalHandler::getPipefd1(); }
    int getPipefd0() const { return SignalHandler::getPipefd0(); }
    void add(int sock) { timerList.add(sock); }
    void adjust(int sock) { timerList.adjust(sock); }
    void remove(int sock) { timerList.remove(sock); }
    bool dealWithSignal(bool &timeout, bool &stop_server) { return SignalHandler::dealWithSignal(timeout, stop_server); }
    const std::vector<int>& getDeath() const { return timerList.getDeath(); }

private:
    SortedTimerList timerList;
};

#endif
