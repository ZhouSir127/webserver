#include <sys/epoll.h>
#include <assert.h>
#include "consts.h"

class Epoll{

private:
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd;
    bool listenET;//true:ET ,false:LT
    bool connectET;    

    int setnonblocking(int fd);

public:
    enum class Type :unsigned char{
        LISTEN,
        PIPE,
        CONNECTION
    };
    
    //对文件描述符设置非阻塞
    
    Epoll(bool listenET,bool connectET):listenET(listenET),connectET(connectET),epollfd(epoll_create(5)){
        assert(epollfd != -1);
    }
    
    ~Epoll(){
        close(epollfd);
    }
    
    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    int  addfd(int fd,Type type);
    int removefd(int fd);
    int modfd(int fd, int ev);
    int wait(int time = -1);

    const epoll_event& getEvent(int idx)const{
        return events[idx];
    }

};