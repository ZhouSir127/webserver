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

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../log/log.h"
#include "consts.h"
#include "../threadpool/threadpool.h"


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
    NO_REQUEST= 0 ,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION,
};

class http_conn
{
public:
    http_conn(bool connectET,int sockfd, string& root,connection_pool&m_connPool,std::unordered_map<std::string,std::string>&m_users)
    :connectET(connectET),m_sockfd(sockfd),doc_root(root),m_connPool(m_connPool),m_users(m_users),m_read_buf(nullptr)
    {
    init();
    }

    ~http_conn() {}

public:
    bool init();
    HTTP_CODE process();
    
    HTTP_CODE read_once();
    bool write();
    bool getLinger() const { return m_linger;}

private:
    void reset_conn();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line();
    HTTP_CODE parse_headers();
    HTTP_CODE do_request();
    HTTP_CODE parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

private:
    std::string line;

    std::mutex m_lock;
    bool m_linger;
    
    std::string& doc_root;
    connection_pool& m_connPool;
    std::unordered_map<std::string,std::string>& m_users;

    int m_sockfd;
    
    std::string m_read_buf;
    int m_read_idx;
    int m_checked_idx;
    int m_start_idx;

    std::string m_write_buf;
    int m_write_idx;
    
    CHECK_STATE m_check_state;
    METHOD m_method;
    std::string m_real_file;
    std::string m_url;
    char *m_host;
    long m_content_length;
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;        //是否启用的POST
    std::string m_string; //存储请求体数据
    int bytes_to_send;
    int bytes_have_send;
    
    bool connectET;
};

class HTTP{

private:

bool connectET;

std::vector<std::unique_ptr<http_conn> > users;
std::string m_root;

connection_pool connPool;
std::unordered_map<std::string, std::string> m_users;

public:
    HTTP(bool connectET,const string& User, const string& passWord, const string& databaseName, int sql_num)
    :connectET(connectET),
    users(1+MAX_FD),
    m_root(std::filesystem::current_path().string()+"/root"),
    connPool("localhost",3306,User,passWord,databaseName,sql_num)
    { 
        //先从连接池中取一个连接
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
    
    bool add_http(int sock){
        if(sock>MAX_FD||users[sock])
            return false;
        users[sock]=make_unique<http_conn>(connectET,sock,m_root,connPool,m_users);
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
