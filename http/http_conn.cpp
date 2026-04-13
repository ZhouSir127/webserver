#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>
#include <algorithm>
#include <memory>
#include <cstdlib>

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";


bool http_conn::init()
{
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    //m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_ptr = m_read_buf;
    m_checked_idx = 0;
    m_read_idx = 0;
    capacity=1024;
    m_write_idx = 0;
    cgi = 0;

    char* new_buf = (char*)realloc(m_read_buf, capacity);
    if (!new_buf)
        return false;

    m_read_buf = new_buf;

    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
HTTP_CODE http_conn::parse_line()
{
    while (m_checked_idx < m_read_idx ){
        if ( m_read_buf[m_checked_idx] == '\r')
        {
            if ( m_checked_idx + 1 == m_read_idx )
                return NO_REQUEST;
            else if ( m_read_buf[m_checked_idx+1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return GET_REQUEST;
            }
            return BAD_REQUEST;
        }
        ++m_checked_idx;
    }
    
    return NO_REQUEST;
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once()
{
    int bytes_read = 0;
    
    //LT读取数据
    if (!connectET){        
        if(m_read_idx == capacity ){
            if(capacity==READ_BUFFER_SIZE)
                return false;
        
            capacity=min(capacity<<1 ,READ_BUFFER_SIZE);
            
            char* new_buf = (char*)realloc(m_read_buf, capacity);
            if (!new_buf) 
                return false;
            
            m_read_buf = new_buf;
        };
    
        bytes_read = recv(m_sockfd, m_read_buf+m_read_idx , capacity-m_read_idx,0);
    
        if (bytes_read < 0 ){
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return true;
            return false;
        }
        else if (bytes_read == 0)
            return false;
        
        m_read_idx += bytes_read;
    }else
        while (true)
        {
            if(m_read_idx == capacity ){
                if(capacity==READ_BUFFER_SIZE)
                    return false;
            
                capacity=min(capacity<<1 ,READ_BUFFER_SIZE);
                char* new_buf = (char*)realloc(m_read_buf, capacity);
                if (!new_buf) 
                    return false;
                
                m_read_buf = new_buf;
            }
    
            bytes_read = recv(m_sockfd, m_read_buf+m_read_idx, capacity-m_read_idx , 0);
            if (bytes_read < 0 )
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if (bytes_read == 0)
                return false;
            
            m_read_idx += bytes_read;
        
        }
    return true;
}

//解析http请求行，获得请求方法，目标url及http版本号
HTTP_CODE http_conn::parse_request_line()
{
    char *method = m_start_ptr;
    
    m_start_ptr=strpbrk(m_start_ptr, " \t");
    if (!m_start_ptr)
        return BAD_REQUEST;
    *m_start_ptr++ = '\0';
    m_start_ptr += strspn(m_start_ptr, " \t");

    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0){
        m_method = POST;
        cgi = 1;
    }else
        return BAD_REQUEST;
    

    m_url = m_start_ptr;

    m_start_ptr=strpbrk(m_start_ptr, " \t");
    if (!m_start_ptr)
        return BAD_REQUEST;
    *m_start_ptr++ = '\0';
    m_start_ptr += strspn(m_start_ptr, " \t");

    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_start_ptr, '/');
    
        if (!m_url)
            return BAD_REQUEST;
    }else if (strncasecmp(m_url, "https://", 8) == 0){
        m_url += 8;
        m_url = strchr(m_url, '/');
        
        if (!m_url)
            return BAD_REQUEST;
    }else if(m_url[0] != '/')
        return BAD_REQUEST;

    char *m_version = m_start_ptr;

    m_start_ptr=m_read_buf+m_checked_idx;

    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;    

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析http请求的一个头部信息
HTTP_CODE http_conn::parse_headers()
{
    if (*m_start_ptr == '\0')//请求头最后的空行
    {
        m_start_ptr=m_read_buf +m_checked_idx;

        if (m_content_length >0 ){
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }else if (strncasecmp(m_start_ptr, "Connection:", 11) == 0){
        m_start_ptr += 11;
        m_start_ptr += strspn(m_start_ptr, " \t");
        if (strcasecmp(m_start_ptr, "keep-alive") == 0)
            m_linger = true;
    }else if (strncasecmp(m_start_ptr, "Content-length:", 15) == 0){
        m_start_ptr += 15;
        m_start_ptr += strspn(m_start_ptr , " \t");
        long len = strtol(m_start_ptr,nullptr,10);
        if ( len < 0 ) 
            return BAD_REQUEST;
        
        m_content_length=len;
    
    }else if (strncasecmp(m_start_ptr, "Host:", 5) == 0){
        m_start_ptr += 5;
        m_start_ptr += strspn(m_start_ptr, " \t");
        m_host = m_start_ptr;
    }else
        return BAD_REQUEST;

    m_start_ptr=m_read_buf + m_checked_idx;
    return NO_REQUEST;
}

HTTP_CODE http_conn::process_read()
{
    if (read_once() == false )
        return BAD_REQUEST;
    while(true)
        if(m_check_state == CHECK_STATE_CONTENT){
            if (m_read_idx - m_checked_idx >= m_content_length )
            {
                m_checked_idx = m_start_ptr-m_read_buf + m_content_length;
                //m_read_buf[m_checked,m_read_idx)已经读到的但尚未check
                //text[0:content_length)为请求体内容
                m_read_buf[m_checked_idx] = '\0';
                //POST请求中最后为输入的用户名和密码
                m_string = m_start_ptr;
                m_start_ptr=m_read_buf +m_checked_idx;
                
                return do_request();
            }
            return NO_REQUEST;
        }else{
            HTTP_CODE line_status = parse_line();
            if (line_status != GET_REQUEST )
                return line_status;
            //[m_start_line,m_check_line)刚分割出来的解析
            if(m_check_state == CHECK_STATE_REQUESTLINE ){
                HTTP_CODE ret = parse_request_line();
                
                if (ret == BAD_REQUEST)
                    break;
            }else{
                HTTP_CODE ret = parse_headers();
                    
                if (ret == BAD_REQUEST)
                    break;
                else if (ret == GET_REQUEST)
                    return do_request();
            }
        }
    return BAD_REQUEST;
}

HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    
    if (strcmp(m_url, "/") == 0)
        snprintf(m_real_file + len, FILENAME_LEN - len, "/judge.html");   
    else{
        const char *p = strrchr(m_url, '/');
        if (!p)
            return BAD_REQUEST;
        
        if (!*++p)
            return BAD_REQUEST;
        
        //处理cgi
        if (cgi == 1 && (*p == '2' || *p  == '3') ){//  /api/2login、/user/3register
            //根据标志判断是登录检测还是注册检测
            snprintf(m_real_file + len, FILENAME_LEN - len, "/%s", p+1);
        
            //将用户名和密码提取出来
            //user=123&passwd=123
            char name[100], password[100];
            int i=0,j=5;
            
            while(i<100 && m_string[j]!='&'){
                name[i] = m_string[j];
                ++i,++j;
            }
            
            if ( i==100 )
                return BAD_REQUEST;
            
            name[i]='\0';

            j+=8;
            i=0;

            while(i < 100 && m_string[j]){
                password[i]=m_string[j];
                ++i,++j;
            }
            
            if(i == 100)
                return BAD_REQUEST;

            password[i]= '\0';

            if (*p == '3')
            {
                //如果是注册，先检测数据库中是否有重名的
                //没有重名的，进行增加数据
                char *sql_insert = (char *)malloc(sizeof(char) * 200);
                strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
                strcat(sql_insert, "'");
                strcat(sql_insert, name);
                strcat(sql_insert, "', '");
                strcat(sql_insert, password);
                strcat(sql_insert, "')");
            
                if (m_users.find(name) == m_users.end() )
                {
                    MYSQL *mysql=nullptr;
                    connectionRAII mysqlcon(&mysql, &m_connPool);
                    
                    m_lock.lock();
                    int res = mysql_query(mysql, sql_insert);
                    m_users[name]=password;
                    m_lock.unlock();

                    if (!res)
                        strcpy(m_url, "/log.html");
                    else
                        strcpy(m_url, "/registerError.html");
                }
                else
                    strcpy(m_url, "/registerError.html");
            }
            //如果是登录，直接判断
            //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
            else if ( *p  == '2')
            {
                if (m_users.find(name) != m_users.end() && m_users[name] == password)
                    strcpy(m_url, "/welcome.html");
                else
                    strcpy(m_url, "/logError.html");
            }
        }
        
        if (*p == '0')
            snprintf(m_real_file + len, FILENAME_LEN - len, "/register.html");
        else if (*p  == '1')
            snprintf(m_real_file + len, FILENAME_LEN - len, "/log.html");
        else if (*p == '5')
            snprintf(m_real_file + len, FILENAME_LEN - len, "/picture.html");
        else if (*p  == '6')
            snprintf(m_real_file + len, FILENAME_LEN - len, "/video.html");
        else if (*p == '7')
            snprintf(m_real_file + len, FILENAME_LEN - len, "/fans.html");    
        else 
            snprintf(m_real_file + len, FILENAME_LEN - len, m_url);
    }

    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_file_stat.st_mode & S_IROTH) )
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
bool http_conn::write()
{
    // int temp = 0;

    // if (bytes_to_send == 0)
    // {
    // //    modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
    //  //   init();
    //     return true;
    // }

    // while (1)
    // {
    //     temp = writev(m_sockfd, m_iv, m_iv_count);

    //     if (temp < 0)
    //     {
    //         if (errno == EAGAIN)
    //         {
    //             modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
    //             return true;
    //         }
    //         unmap();
    //         return false;
    //     }

    //     bytes_have_send += temp;
    //     bytes_to_send -= temp;
    //     if (bytes_have_send >= m_iv[0].iov_len)
    //     {
    //         m_iv[0].iov_len = 0;
    //         m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
    //         m_iv[1].iov_len = bytes_to_send;
    //     }
    //     else
    //     {
    //         m_iv[0].iov_base = m_write_buf + bytes_have_send;
    //         m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
    //     }

    //     if (bytes_to_send <= 0)
    //     {
    //         unmap();
    //        //modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

    //         if (m_linger)
    //         {
    //         //    init();
    //             return true;
    //         }
    //         else
    //         {
    //             return false;
    //         }
    //     }
    // }
}
bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);

    return true;
}
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

HTTP_CODE http_conn::process(){

    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
        return NO_REQUEST;
    
    if ( !process_write(read_ret) )
        return BAD_REQUEST;

    return GET_REQUEST;
}

