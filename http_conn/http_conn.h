#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
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
#include <sys/wait.h>
#include <sys/uio.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <semaphore.h>
#include "../CGImysql/sql_connection_pool.h"
#include "../log/log.h"
#include "../consts.h"
//#include "../thread_pool/thread_pool.h"

#include <sstream>  
#include <utility>   
#include <cstdio>  

#include <mavsdk/mavsdk.h>
#include <mavsdk/system.h>

#include <mavsdk/plugin_base.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>

extern std::shared_ptr<mavsdk::Mavsdk> mavsdkPtr;
extern std::shared_ptr<mavsdk::System> drone;
extern std::shared_ptr<mavsdk::Action> action;
extern std::shared_ptr<mavsdk::Telemetry> telemetry;

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

class User{
private:
    std::unordered_map<std::string, std::string> m_users;
    std::mutex lock;
public:
    User(connection_pool&connPool){
        MYSQL *mysql = nullptr;
        connectionRAII mysqlcon(mysql,&connPool);
    
        //在user表中检索username，passwd数据，浏览器端输入
        if (mysql_query(mysql, "SELECT username,passwd FROM user"))
            LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
        
        //从表中检索完整的结果集
        MYSQL_RES *result = mysql_store_result(mysql);

        if(!result)
            throw std::exception();
        //从结果集中获取下一行，将对应的用户名和密码，存入map中
        while (MYSQL_ROW row = mysql_fetch_row(result))
            m_users[row[0] ] = row[1];
    
        mysql_free_result(result); 
    }
    void add(const std::string&name,const std::string&password){
        std::unique_lock<std::mutex>Lock(lock);
        m_users[name]=password;
    }
    bool exists(const std::string&name){
        std::unique_lock <std::mutex>Lock(lock);
        return m_users.find(name) != m_users.end();
    }
    bool check(const std::string&name,const std::string&password){
        std::unique_lock <std::mutex>Lock(lock);
        return m_users[name] == password;
    }
};

class HttpConn
{
private:
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
    const int socketFd;
    connection_pool& connPool;
    User& users; 
    
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
    HttpConn(bool connectET,int sockfd,connection_pool&m_connPool,User&m_users,const std::string&root)
    :isConnectEt(connectET),socketFd(sockfd),connPool(m_connPool),users(m_users),
    readIdx(0),checkedIdx(0),startIdx(0),
    checkState(CheckState::CHECK_STATE_REQUESTLINE),method(HttpMethod::GET),isLinger(false),contentLength(0),
    root(root),
    bytesToSend(0),bytesHaveSent(0),ioVectorCount(1),ioVectorIdx(0),fileAddress(nullptr),fileSize(0)
    {}

    void init();
    HttpCode process();
    HttpCode write();
    bool getLinger() const { return isLinger;}
};

class Http{

private:

bool connectET;
std::vector<std::unique_ptr<HttpConn> > users;
connection_pool connPool;
User m_users;
const std::string&root;

public:
    Http(bool connectET,const std::string&IP,int port,const std::string& User, const std::string& passWord, const std::string& databaseName, int sql_num,const std::string&root)
    :connectET(connectET),
    users(1+consts::MAX_FD),
    connPool(IP,port,User,passWord,databaseName,sql_num),
    m_users(connPool),
    root(root)
    {}
    
    void add(int sock){
        users[sock]=std::make_unique<HttpConn>(connectET,sock,connPool,m_users,root);
    }    
    //关闭连接，关闭一个连接，客户总量减一
    void remove(int sock){
        users[sock].reset();
    }

    HttpCode process(int sock){
        return users[sock]->process();
    } 

    HttpCode write(int sock){
        return users[sock]->write();
    }

    bool getLinger(int sock){
        return users[sock]->getLinger();
    }

    void init(int sock){
        users[sock]->init();
    }
};


#endif 
