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

#include <vector>
#include <memory>
#include <mutex>
#include "../consts.h"
#include "../args.h"

class SortedTimerList
{
private:

    struct Node{    
        Node():expire(0),fd(-1){}
        Node(time_t expire,int fd):expire(expire),fd(fd){}
        
        time_t expire;
        int fd;
        
        std::weak_ptr<Node>prev;
        std::weak_ptr<Node>next;
    };

    std::mutex lock;
    time_t lifeSpan;
    std::shared_ptr<Node> head;
    std::shared_ptr<Node> tail;
    std::vector<std::shared_ptr<Node> > fdToNode;    
    std::vector<int> death;

public:
    SortedTimerList(time_t lifeSpan):lifeSpan(lifeSpan),head(std::make_shared<Node>()),tail(std::make_shared<Node>()),fdToNode(consts::MAX_FD+1){
        head->next=tail;
        tail->prev=head;
    }
    
    void add(int fd);
    void adjust(int fd);
    void remove(int fd);
    void tick();
    const std::vector<int>& getDeath() const { return death; }
    
    //bool exist(int fd)const { return (bool)users[fd];}
};


class SignalHandler{

public:
    SignalHandler() = delete;

    static int getPipeWriteFd() { return pipefd[1]; }   
    static int getPipeReadFd() { return pipefd[0]; }

    static void setAlarm()  { alarm(timeSlot); }
    static void dealWithSignal (bool &timeout, bool &stopServer);

    static void init(unsigned int slot){
        timeSlot = slot;
        socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    
        addSig(SIGPIPE, SIG_IGN);
        addSig(SIGALRM, sigHandler);
        addSig(SIGTERM, sigHandler);

        setAlarm();
    }
    static void clean() {
        close(pipefd[1]);
        close(pipefd[0]);
    }

private:
    static unsigned int timeSlot;
    static int pipefd[2];
    //设置信号函数
    static void addSig(int sig, void(*handler)(int), bool restart = false );
    //信号处理函数
    static void sigHandler(int sig);
};

class TimerManager{

public:
    TimerManager(const TimerInfo& timerInfo):list(timerInfo.lifeSpan){
        SignalHandler::init(timerInfo.timeSlot);
    }
    ~TimerManager(){
        SignalHandler::clean();
    }
        //定时处理任务，重新定时以不断触发SIGALRM信号
    void timerHandler(){
        list.tick();
        SignalHandler::setAlarm();
    }
    int getPipeWriteFd() const { return SignalHandler::getPipeWriteFd(); }
    int getPipeReadFd() const { return SignalHandler::getPipeReadFd(); }
    void add(int fd) { list.add(fd); }
    void adjust(int fd) { list.adjust(fd); }
    void remove(int fd) { list.remove(fd); }
    void dealWithSignal(bool &timeout, bool &stop_server) { SignalHandler::dealWithSignal(timeout, stop_server); }
    const std::vector<int>& getDeath() const { return list.getDeath(); }

private:
    SortedTimerList list;
};

#endif
