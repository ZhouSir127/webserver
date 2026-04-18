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

#include <mavsdk/mavsdk.h>
#include <mavsdk/system.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>

extern mavsdk::Mavsdk mavsdk;
extern std::shared_ptr<mavsdk::System> drone;
extern std::shared_ptr<mavsdk::Action> action;
extern std::shared_ptr<mavsdk::Telemetry> telemetry;

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

    const std::string&root;
    std::string m_real_file;

    std::string m_write_buf;
    int bytes_to_send;
    int bytes_have_send;
    
    struct iovec m_iv[2];
    int m_iv_count;
    int m_iv_idx;

    char* m_file_address; // 专门用来记录 mmap 的原地址
    size_t m_file_size;   // 专门记录文件大小

public:
    http_conn(bool connectET,int sockfd,connection_pool&m_connPool,User&m_users,const std::string&root)
    :connectET(connectET),m_sockfd(sockfd),m_connPool(m_connPool),m_users(m_users),root(root),
    m_read_idx(0),m_checked_idx(0),m_start_idx(0),
    m_check_state(CHECK_STATE_REQUESTLINE),m_method(GET),m_linger(false),m_content_length(0),
    bytes_to_send(0),bytes_have_send(0),m_iv_count(1),m_iv_idx(0),m_file_address(nullptr),m_file_size(0)
    {}

    void init();
    HTTP_CODE process();
    HTTP_CODE write();
    bool getLinger() const { return m_linger;}
};

class HTTP{

private:

bool connectET;
std::vector<std::unique_ptr<http_conn> > users;
connection_pool connPool;
User m_users;
const std::string&root;

public:
    HTTP(bool connectET,const std::string&IP,int port,const std::string& User, const std::string& passWord, const std::string& databaseName, int sql_num,const std::string&root)
    :connectET(connectET),
    users(1+consts::MAX_FD),
    connPool(IP,port,User,passWord,databaseName,sql_num),
    m_users(connPool),
    root(root)
    {}
    
    void add_http(int sock){
        users[sock]=std::make_unique<http_conn>(connectET,root,sock,connPool,m_users,root);
    }    
    //关闭连接，关闭一个连接，客户总量减一
    void close_conn(int sockfd){
        users[sockfd].reset();
    }

    HTTP_CODE process(int fd){
        return users[fd]->process();
    } 

    HTTP_CODE write(int fd){
        return users[fd]->write();
    }

    bool isLinger(int fd){
        return users[fd]->getLinger();
    }

    void init(int fd){
        users[fd]->init();
    }
};


#endif 
