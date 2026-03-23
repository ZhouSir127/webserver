#include <sys/epoll.h>
#include <assert.h>

class Epoll{

public:
    enum class Type :unsigned char{
        LISTEN,
        PIPE,
        CONNECTION
    };
    
    //对文件描述符设置非阻塞
    static int setnonblocking(int fd);

    Epoll(bool listenET,bool connectET):listenET(listenET),connectET(connectET),epollfd(epoll_create(5)){
        assert(epollfd != -1);
    }
    
    ~Epoll(){
        close(epollfd);
    }
    
    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int fd,Type type);
    void removefd(int fd);
    void modfd(int fd, int ev);
    int wait(int timeout = -1);


    bool isListenEvent(int idx,int m_listenfd) const;    
    bool isSignal(int idx,int pipeFd0) const;
    int isConnectionEvent(int idx) const;
    int isErrorEvent(int idx) const;

private:
    static const int MAX_EVENT_NUMBER = 10000; //最大事件数
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd;
    bool listenET;//true:ET ,false:LT
    bool connectET;    

};