#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <semaphore.h>
#include "../log/log.h"
#include "../consts.h"

#include <sstream>  
#include <utility>   
#include <cstdio>  

#include "../router/router.h"
#include "../args.h"
#include "../channel/channel.h"  
#include "../work_queue/work_queue.h"
#include "../epoll_manager/epoll_manager.h"
#include "../set/set.h"

enum class HttpMethod
{
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT,
    PATH
};
enum class CheckState
{
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER,
    CHECK_STATE_CONTENT
};
enum class HttpCode
{
    NO_REQUEST = 0 ,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,    
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    CLOSED_CONNECTION,
    INTERNAL_ERROR
};

class Message{

private:

static std::unordered_map<int,std::string> form ;
static std::unordered_map<int,std::string> title;

HttpCode processRead();
HttpCode parseLine();
HttpCode parseRequestLine();
HttpCode parseHeaders();
HttpCode prepareFile();
bool processWrite(HttpCode);
HttpCode process(Router&);
HttpCode write(bool,int);

template<typename... Args>
bool addResponse(Args&&... args ){
    
    std::stringstream ss;
    (ss  << ... << std::forward<Args>(args) ); 

    const std::string& result = ss.str();

    if (writeBuffer.size() + result.size() > consts::WRITE_BUFFER_SIZE)
        return false;

    writeBuffer.append(result);

    return true;
}

std::string readBuffer;
size_t checkedIdx;
size_t startIdx;

CheckState checkState;
std::string line;

HttpMethod method;
bool isLinger;
std::string url;
size_t contentLength;

std::string cookie; 
std::string requestBody;

std::string realFilePath;
std::string token;

std::string writeBuffer;
struct iovec ioVectors[2];
int ioVectorCount;
int ioVectorIdx;

char* fileAddress; // 专门用来记录 mmap 的原地址
size_t fileSize;   // 专门记录文件大小

public:
    Message():readBuffer(1024,'\0'),checkedIdx(0),startIdx(0),
    checkState(CheckState::CHECK_STATE_REQUESTLINE),method(HttpMethod::GET),isLinger(true),contentLength(0),
    ioVectorCount(1),ioVectorIdx(0),fileAddress(nullptr),fileSize(0)
    {}

    ~Message(){
        if (fileAddress) {
            munmap(fileAddress, fileSize);
            fileAddress = nullptr;
        }
    }
    void setFile(std::string&& str){realFilePath = str;}
    const std::string& getBody()const {return requestBody;}
    const std::string& getURL() const{return url;}
    HttpMethod getMethod() const {return method;}
    const std::string&getCookie()const {return cookie;}
    void setToken(const std::string&token) {this -> token = token;}
};

class HttpConn
{
private:    
    const bool isConnectEt;
    const int fd;
    WorkQueue<std::shared_ptr<HttpConn> >& workQueue;
    std::unique_ptr<Channel>httpChannel;
    bool isLinger;
    Router&router;

public:
    HttpConn(bool connectET,int fd,WorkQueue<std::shared_ptr<HttpConn> >& workQueue,Router&router)
    :isConnectEt(connectET),fd(fd),workQueue(workQueue),isLinger(true),router(router)
    {}
    ~HttpConn(){
        EpollManager::getInstance().remove(httpChannel.get() );
        close(fd);
    }

    HttpCode read();
    Channel*getChannel() const { return httpChannel.get(); }
    int getFd() const {return fd;}
    void setChannel(const std::shared_ptr<HttpConn>&self){
        httpChannel = std::make_unique<Channel>(
            fd,
            [this,self]() ->void { ;this->workQueue.append(self); },
            [this,self]() ->void { this->workQueue.append(self); },
            [this,self]() ->void { this->workQueue.append(self); }
        );
    EpollManager::getInstance().add(httpChannel.get(),EPOLLIN | EPOLLRDHUP | EPOLLONESHOT | (isConnectEt ? EPOLLET : static_cast<uint32_t>(0)) );
    }
    bool getConnectEt() const { return isConnectEt; }
};


class HttpManager{

private:

bool isConnectEt;
std::vector<std::shared_ptr<HttpConn> > fdToConn;
Router router;
WorkQueue<std::shared_ptr<HttpConn> >& workQueue;

public:
    HttpManager(const HttpInfo& httpInfo,const SqlInfo& sqlInfo,const RedisInfo& redisInfo,WorkQueue<std::shared_ptr<HttpConn> >& workQueue)
    :isConnectEt(httpInfo.isConnectEt),
    fdToConn(1+consts::MAX_FD),
    router(sqlInfo,redisInfo,httpInfo.root),
    workQueue(workQueue)
    {}
    
    void add(int fd){
        fdToConn[fd]=std::make_shared<HttpConn>(isConnectEt,fd,workQueue,router);
        fdToConn[fd]->setChannel(fdToConn[fd]);
    }    
    //关闭连接，关闭一个连接，客户总量减一
    void remove(int fd){
        if(fdToConn[fd])
            fdToConn[fd].reset();
    }

};


#endif 
