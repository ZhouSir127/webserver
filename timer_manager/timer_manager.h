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
#include <mutex>
#include <memory>

#include "../consts.h"
#include "../args.h"
#include "../epoll_manager/epoll_manager.h"
#include "../channel/channel.h"
#include "../death/death.h"

class TimerMinHeap
{
private:
    std::mutex lock;
    time_t lifeSpan;
    std::vector<std::pair<time_t, size_t> > fdToExpireIdx;//{死期，在堆索引}    
    size_t size;
    std::vector<int>heap;
    Death&death;
    bool keep(size_t idx);
    void pop();

public:
    TimerMinHeap(time_t lifeSpan, Death& death):lifeSpan(lifeSpan),fdToExpireIdx(consts::MAX_FD+1,{-1,-1} ),size(0),heap(consts::MAX_FD+1,-1),death(death){}
    
    void add(int fd);
    void adjust(int fd);
    void remove(int fd);
    void tick();
};

class SignalHandler{

public:
    SignalHandler() = delete;

    static void setAlarm()  { alarm(timeSlot); }
    static void dealWithSignal ();

    static void init(unsigned int slot){
        timeSlot = slot;
        socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
        
        timeout = false;
        stopServer = false;

        signalChannel = std::make_unique<Channel>(
            pipefd[0],
            EPOLLIN,
            [](){ dealWithSignal(); },
            nullptr,
            nullptr
        );

        EpollManager::getInstance().add(signalChannel.get());

        addSig(SIGPIPE, SIG_IGN);
        addSig(SIGALRM, sigHandler);
        addSig(SIGTERM, sigHandler);

        setAlarm();
    }
    static void clean() {
        close(pipefd[1]);
        close(pipefd[0]);
    }

    static bool getTimeout() { 
        bool ret = timeout;    
        timeout = false;
        return ret; 
    }
    static bool getStopServer() { 
        bool ret = stopServer;
        stopServer = false;
        return ret; 
    }

private:
    static unsigned int timeSlot;
    static int pipefd[2];
    //设置信号函数
    static void addSig(int sig, void(*handler)(int), bool restart = false );
    //信号处理函数
    static void sigHandler(int sig);
    static bool timeout;
    static bool stopServer;
    static std::unique_ptr<Channel> signalChannel;
    
};

class TimerManager{

public:
    TimerManager(const TimerInfo& timerInfo, Death& death):heap(timerInfo.lifeSpan, death){
        SignalHandler::init(timerInfo.timeSlot);
    }
    ~TimerManager(){
        SignalHandler::clean();
    }
        //定时处理任务，重新定时以不断触发SIGALRM信号
    void timerHandler(){
        heap.tick();
        SignalHandler::setAlarm();
    }

    void add(int fd) { heap.add(fd); }
    void adjust(int fd) { heap.adjust(fd); }
    void remove(int fd) { heap.remove(fd); }
    bool getTimeout() const { return SignalHandler::getTimeout(); }
    bool getStopServer() const { return SignalHandler::getStopServer(); }

private:
    TimerMinHeap heap;
};

#endif
