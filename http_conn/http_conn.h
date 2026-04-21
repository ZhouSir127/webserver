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
    CLOSED_CONNECTION
};


class HttpConn
{
private:
    friend class Router;
    static std::unordered_map<int,std::string> form ;
    static std::unordered_map<int,std::string> title;

    HttpCode processRead();
    HttpCode readOnce();
    HttpCode parseLine();
    HttpCode parseRequestLine();
    HttpCode parseHeaders();
    HttpCode doRequest();
    
    bool processWrite(HttpCode ret);
    
    template<typename... Args>
    bool addResponse(Args&&... args ){
        
        std::stringstream ss;
        (ss  << ... << std::forward<Args>(args) ); 

        std::string result = ss.str();

        if (writeBuffer.size() + result.size() > consts::WRITE_BUFFER_SIZE)
            return false;

        writeBuffer.append(result);

        LOG_INFO("request:%s", writeBuffer.c_str());

        return true;
    }
    
    const bool isConnectEt;
    const int fd;
    
    std::string readBuffer;
    size_t readIdx;
    size_t checkedIdx;
    size_t startIdx;

    CheckState checkState;
    std::string line;
    HttpMethod method;
    std::string url;
    bool isLinger;
    size_t contentLength;
    std::string requestBody;

    Router &router;
    const std::string&root;
    std::string realFilePath;

    std::string writeBuffer;
    size_t bytesToSend;
    size_t bytesHaveSent;
    
    struct iovec ioVectors[2];
    int ioVectorCount;
    int ioVectorIdx;

    char* fileAddress; // 专门用来记录 mmap 的原地址
    size_t fileSize;   // 专门记录文件大小

public:
    HttpConn(bool connectET,int fd,Router&router,const std::string&root)
    :isConnectEt(connectET),fd(fd),
    readBuffer(1024,'\0'),readIdx(0),checkedIdx(0),startIdx(0),
    checkState(CheckState::CHECK_STATE_REQUESTLINE),method(HttpMethod::GET),isLinger(false),contentLength(0),
    router(router),root(root),
    bytesToSend(0),bytesHaveSent(0),ioVectorCount(1),ioVectorIdx(0),fileAddress(nullptr),fileSize(0)
    {}
    ~HttpConn(){
        if (fileAddress) {
            munmap(fileAddress, fileSize);
            fileAddress = nullptr;
        }
    }

    void init();
    HttpCode process();
    HttpCode write();
    bool getLinger() const { return isLinger;}
};


class HttpManager{

private:

bool isConnectEt;
std::vector<std::unique_ptr<HttpConn> > fdToConn;
Router router;
const std::string&root;

public:
    HttpManager(const HttpInfo& httpInfo,const SqlInfo& sqlInfo)
    :isConnectEt(httpInfo.isConnectEt),
    fdToConn(1+consts::MAX_FD),
    router(sqlInfo),
    root(httpInfo.root)
    {}
    
    void add(int fd){
        fdToConn[fd]=std::make_unique<HttpConn>(isConnectEt,fd,router,root);
    }    
    //关闭连接，关闭一个连接，客户总量减一
    void remove(int fd){
        fdToConn[fd].reset();
    }

    HttpCode process(int fd){
        return fdToConn[fd]->process();
    } 

    HttpCode write(int fd){
        return fdToConn[fd]->write();
    }

    bool getLinger(int fd){
        return fdToConn[fd]->getLinger();
    }

    void init(int fd){
        fdToConn[fd]->init();
    }
};


#endif 
