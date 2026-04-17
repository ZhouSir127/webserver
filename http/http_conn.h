#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
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
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <unordered_map>
#include <vector>
#include <memory>
#include <filesystem>
#include <string>
#include <unordered_map>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../log/log.h"
#include "../consts.h"
#include "../threadpool/threadpool.h"

#include <sstream>  
#include <utility>   
#include <cstdio>  

//#include "MAVSDK/cpp/src/mavsdk/core/include/mavsdk/mavsdk.hpp"
// #include "MAVSDK/cpp/src/mavsdk/core/include/mavsdk/system.hpp"
// #include "MAVSDK/cpp/src/mavsdk/plugins/action/include/plugins/action/action.hpp"
// #include "MAVSDK/cpp/src/mavsdk/plugins/telemetry/include/plugins/telemetry/telemetry.hpp"
// #include "MAVSDK/cpp/src/mavsdk/plugins/mission/include/plugins/mission/mission.hpp"

// 仅仅是声明，使用 extern
// extern mavsdk::Mavsdk mavsdk;
// extern std::shared_ptr<mavsdk::System> drone;
// extern std::shared_ptr<mavsdk::Action> action;
// extern std::shared_ptr<mavsdk::Telemetry> telemetry;


enum METHOD
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
enum CHECK_STATE
{
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER,
    CHECK_STATE_CONTENT
};
enum HTTP_CODE
{
    NO_REQUEST = 0 ,
    GET_REQUEST,
    BAD_REQUEST,

    NO_RESOURCE,    
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    CLOSED_CONNECTION,
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
    void add(const string&name,const string&password){
        std::unique_lock<std::mutex>Lock(lock);
        m_users[name]=password;
    }
    bool exists(const string&name){
        std::unique_lock <std::mutex>Lock(lock);
        return m_users.find(name) != m_users.end();
    }
    bool check(const string&name,const string&password){
        std::unique_lock <std::mutex>Lock(lock);
        return m_users[name] == password;
    }
};

class http_conn
{
private:
    static std::unordered_map<int,std::string> form ;
    static std::unordered_map<int,std::string> title;

    HTTP_CODE process_read();
    HTTP_CODE read_once();
    HTTP_CODE parse_line();
    HTTP_CODE parse_request_line();
    HTTP_CODE parse_headers();
    HTTP_CODE do_request();
    
    bool process_write(HTTP_CODE ret);
    
    template<typename... Args>
    bool add_response(Args&&... args ){
        
        std::stringstream ss;
        (ss  << ... << std::forward<Args>(args) ); 

        std::string result = ss.str();

        if (m_write_buf.size() + result.size() > WRITE_BUFFER_SIZE)
            return false;

        m_write_buf.append(result);

        LOG_INFO("request:%s", m_write_buf.c_str());

        return true;
    }
    
    const bool connectET;
    const int m_sockfd;
    connection_pool& m_connPool;
    User& m_users; 
    
    std::string m_read_buf;
    int m_read_idx;
    int m_checked_idx;
    int m_start_idx;


    CHECK_STATE m_check_state;
    std::string line;
    METHOD m_method;
    std::string m_url;
    bool m_linger;
    long m_content_length;
    std::string m_string;

    std::string m_real_file;

    std::string m_write_buf;
    int bytes_to_send;
    int bytes_have_send;
    
    struct iovec m_iv[2];
    int m_iv_count;

    char* m_file_address; // 专门用来记录 mmap 的原地址
    size_t m_file_size;   // 专门记录文件大小
public:
    http_conn(bool connectET,int sockfd,connection_pool&m_connPool,User&m_users)
    :connectET(connectET),m_sockfd(sockfd),m_connPool(m_connPool),m_users(m_users),
    m_read_idx(0),m_checked_idx(0),m_start_idx(0),
    m_check_state(CHECK_STATE_REQUESTLINE),m_method(GET),m_linger(false),m_content_length(0),
    bytes_to_send(0),bytes_have_send(0),m_iv_count(1),m_file_address(nullptr),m_file_size(0)
    {}

    bool init();
    HTTP_CODE process();
    bool write();
    bool getLinger() const { return m_linger;}
};

class HTTP{

private:

bool connectET;

std::vector<std::unique_ptr<http_conn> > users;

connection_pool connPool;

User m_users;

public:
    HTTP(bool connectET,const string& User, const string& passWord, const string& databaseName, int sql_num)
    :connectET(connectET),
    users(1+MAX_FD),
    connPool("localhost",3306,User,passWord,databaseName,sql_num),
    m_users(connPool)
    {}
    
    bool add_http(int sock){
        if(sock>MAX_FD||users[sock])
            return false;
        users[sock]=make_unique<http_conn>(connectET,sock,connPool,m_users);
        return true;
    }    
    //关闭连接，关闭一个连接，客户总量减一
    bool close_conn(int sockfd)
    {
        if(sockfd>MAX_FD||users[sockfd]==nullptr )
            return false;
        
        users[sockfd].reset();
        
        return true;
    }

    HTTP_CODE process(int fd){
        return users[fd]->process();
    } 

    bool write(int fd){
        if(fd>MAX_FD||users[fd]==nullptr )
            return false;
        
        return users[fd]->write();
    }

    bool isLinger(int fd){
        if(fd>MAX_FD||users[fd]==nullptr )
            return false;

        return users[fd]->getLinger();
    }

    bool init(int fd){
        if(fd>MAX_FD||users[fd]==nullptr )
            return false;
        return users[fd]->init();
    }
};


#endif
