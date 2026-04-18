#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>
#include <algorithm>
#include <memory>
#include <cstdlib>
#include <string>
#include <sstream>
#include "../consts.h"

mavsdk::Mavsdk mavsdk;
std::shared_ptr<mavsdk::System> drone = nullptr;
std::shared_ptr<mavsdk::Action> action = nullptr;
std::shared_ptr<mavsdk::Telemetry> telemetry = nullptr;

std::unordered_map<int,std::string> form {
    {400,"Your request has bad syntax or is inherently impossible to staisfy.\n"}, 
    {403,"You do not have permission to get file form this server.\n"},
    {404,"The requested file was not found on this server.\n"},
    //{500,"There was an unusual problem serving the request file.\n"}
};    
std::unordered_map<int,std::string> title {
    {200,"OK"},
    {400,"Bad Request"}, 
    {403,"Forbidden"},
    {404,"Not Found"},
    //{500,"Internal Error"}
};    

void http_conn::init()
{
    m_read_buf.resize(1024);
    m_read_idx = 0;
    m_checked_idx = 0;
    m_start_idx = 0;

    m_check_state = CHECK_STATE_REQUESTLINE;
    line.clear();
    m_method = GET;
    m_url.clear();
    m_linger = false;
    m_content_length = 0;
    m_string.clear();

    m_real_file.clear();
    
    m_write_buf.clear();
    bytes_to_send = 0;
    bytes_have_send = 0;

    m_iv_count = 1;
    m_iv_idx = 0;

    if (m_file_address) {
        munmap(m_file_address, m_file_size);
        m_file_address = nullptr;
    }
    m_file_size = 0;
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
            else if ( m_read_buf[m_checked_idx+1] == '\n'){

                line = std::string(m_read_buf.begin() + m_start_idx , m_read_buf.begin() + m_checked_idx);
                
                m_checked_idx += 2;
                m_start_idx = m_checked_idx;

                return GET_REQUEST;
            }
            return BAD_REQUEST;//'\r后非'\n'
        }
        ++m_checked_idx;
    }
    
    return NO_REQUEST;//没读到'/r'
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
HTTP_CODE http_conn::read_once()
{
    //LT读取数据
    if (!connectET){
        if(m_read_buf.size()==m_read_idx){
            if(m_read_buf.size() == READ_BUFFER_SIZE)
                return BAD_REQUEST;
            m_read_buf.resize( min( (m_read_buf.size()<<1),(size_t)READ_BUFFER_SIZE) );
        }
        int bytes_read = recv(m_sockfd, &m_read_buf[m_read_idx] ,m_read_buf.size()-m_read_idx,0);
    
        if (bytes_read < 0 ){
            if (errno == EAGAIN || errno == EWOULDBLOCK)//无数据可读
                return NO_REQUEST;
            return BAD_REQUEST;//读故障
        }else if (bytes_read == 0)//对方正常关闭了连接
            return CLOSED_CONNECTION;
        
        m_read_idx += bytes_read;
    }else
        while (true)
        {    
            if(m_read_buf.size()==m_read_idx)
                m_read_buf.resize(m_read_buf.size()<<1 );
            
            int bytes_read = recv(m_sockfd, &m_read_buf[m_read_idx], m_read_buf.size()-m_read_idx , 0);
            if (bytes_read < 0 ){
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return BAD_REQUEST;
            }else if (bytes_read == 0)
                return CLOSED_CONNECTION;
            
            m_read_idx += bytes_read;
        }
    return GET_REQUEST;
}

//解析http请求行，获得请求方法，目标url及http版本号
HTTP_CODE http_conn::parse_request_line()
{
    std::istringstream iss(line);
    std::string token;

    if(!(iss >> token) )
        return BAD_REQUEST;

    if ( token.compare("GET") == 0 )
        m_method = GET;
    else if (  token.compare("POST") == 0 )
        m_method = POST;
    else
        return BAD_REQUEST;

    if(!(iss >> token) )
        return BAD_REQUEST;
    
    int pos(0);

    if ( token.compare(0 , 7 ,"http://") == 0){
        pos = token.find_first_of('/',7);
    
        if (pos == std::string::npos )
            return BAD_REQUEST;
    }else if (token.compare(0, 8 ,"https://") == 0){
        pos = token.find_first_of('/',8);
    
        if ( pos == std::string::npos )
            return BAD_REQUEST;
    }else if(token[0] != '/')
        return BAD_REQUEST;

    m_url = token;

    if(!(iss >> token) )
        return BAD_REQUEST;

    if (token.compare(pos , 0 ,"HTTP/1.1") != 0)
        return BAD_REQUEST;    

    m_check_state = CHECK_STATE_HEADER;
    return GET_REQUEST;
}

//解析http请求的一个头部信息
HTTP_CODE http_conn::parse_headers()
{
    if ( line.empty() )//请求头最后的空行
    {
        m_check_state = CHECK_STATE_CONTENT;       
        return GET_REQUEST;
    }else if (line.compare(0, 11 ,"Connection:") == 0){
        int pos = line.find_first_not_of ( " \t" , 11 );
        if(pos == std::string::npos)
            return BAD_REQUEST;
        if ( line.compare(pos, 10 , "keep-alive") == 0)
            m_linger = true;
    }else if (line.compare(0 , 15 , "Content-length:") == 0){
        int pos = line.find_first_not_of ( " \t" , 15 );
        if(pos == std::string::npos)
            return BAD_REQUEST;

        m_content_length = stol(std::string(line.begin()+pos , line.end() ) );
        if ( m_content_length < 0 ) 
            return BAD_REQUEST;
    }

    return GET_REQUEST;
}

HTTP_CODE http_conn::process_read()
{
    HTTP_CODE ret = read_once();
    if ( ret != GET_REQUEST )//对方关闭了连接或读故障或缓冲大小上限或
        return ret;
    
    while(true)
        if(m_check_state == CHECK_STATE_CONTENT){
            if (m_content_length == 0) 
                return do_request();
            else if (m_read_idx - m_checked_idx >= m_content_length )
            {
                //m_read_buf[m_checked,m_read_idx)已经读到的但尚未check
                //text[0:content_length)为请求体内容
                m_checked_idx = m_start_idx + m_content_length ;
                //POST请求中最后为输入的用户名和密码
                m_string = std::string(m_read_buf.begin() + m_start_idx ,m_read_buf.begin() + m_checked_idx);
                m_start_idx = m_checked_idx = m_read_idx;
                
                return do_request();
            }
            
            return NO_REQUEST;
        }else{
            HTTP_CODE ret = parse_line();
            if (ret != GET_REQUEST )
                return ret;
            
            if(m_check_state == CHECK_STATE_REQUESTLINE ){
                HTTP_CODE ret = parse_request_line();
                if ( ret != GET_REQUEST)
                    return ret;
            }else if (parse_headers() == BAD_REQUEST)
                return BAD_REQUEST;
        }
    return BAD_REQUEST;
}

HTTP_CODE http_conn::do_request()
{
    if(m_method == POST){
        std::string name , password;
        std::istringstream iss(m_string);
        
        if( !(iss >> name) )
            return BAD_REQUEST;
        if( !(iss >> password) )
            return BAD_REQUEST;
    
        if (m_url[1] == 0){    //注册
            if (m_users.exists(name) ==false ){
                std::string sql_insert = "INSERT INTO user(username, passwd) VALUES('" 
                        + name 
                        + "', '" 
                        + password 
                        + "')";

                MYSQL *mysql=nullptr;
                connectionRAII mysqlcon(mysql, &m_connPool);
                
                if (mysql_query(mysql, sql_insert.data() ) == 0){
                    m_real_file = std::string(doc_root + "/log.html");
                    m_users.add(name,password);
                }else
                    m_real_file = std::string(doc_root + "/registerError.html");
            }else
                m_real_file = std::string(doc_root + "/registerError.html");
    
        }else if ( m_users.check(name,password) )
            m_real_file = std::string(doc_root + "/index.html");
        else
            m_real_file = std::string("doc_root + /logError.html");    
    }else{
        if(m_url.size() == 1)
            m_real_file = std::string(doc_root + "/log.html" );
        else
            switch (m_url[1]){
                case 0:
                    m_real_file = std::string(doc_root + "/register.html" );
                    break;
                case 1:
                    mavsdk.add_any_connection("udp://:14540");
                    // 简易处理：等待飞控连接
                    usleep(100000); 
                    if (mavsdk.systems().size() > 0) {
                        drone = mavsdk.systems().at(0);
                        action = std::make_shared<mavsdk::Action>(drone);
                        telemetry = std::make_shared<mavsdk::Telemetry>(drone);
                    }
                  break;
                case 2:// /2 -> /disconnect
                    action = nullptr;
                    telemetry = nullptr;
                    drone == nullptr;
                    LOG_INFO("无人机连接已断开");
                    break;
                case 3:
                    // /3 -> connstatus
                    if (drone) {
                        bool is_conn = drone->is_connected();
                        LOG_INFO("连接状态: %d", is_conn);
                    }
                    break;
                case 4: // /4 -> arm
                    if (action) action->arm();
                    break;
                case 5: // /5 -> disarm
                    if (action) action->disarm();
                    break;
                case 6: // /6 -> mode (切换为 hold 模式演示)
                    if (action) action->hold(); 
                    break;
                case 7: // /7 -> takeoff
                    if (action) action->takeoff();
                    break;
                case 8: // /8 -> land
                    if (action) action->land();
                    break;
                case 9: // /9 -> rtl
                    if (action) action->return_to_launch();
                    break;
                case 10: // /10 -> goto
                    // goto 需要额外的经纬度参数，通常前端发 /10?lat=xx&lon=xx，此处预留
                    break;
                case 11: // /11 -> position
                    if (telemetry) telemetry->position();
                    break;
                case 12: // /12 -> velocity
                    if (telemetry) telemetry->velocity_ned();
                    break;
                case 13: // /13 -> attitude
                    if (telemetry) telemetry->attitude_euler();
                    break;
                case 14: // /14 -> battery
                    if (telemetry) telemetry->battery();
                    break;
                case 15: // /15 -> gps
                    if (telemetry) telemetry->gps_info();
                    break;
                case 16: // /16 -> state
                    if (telemetry) telemetry->health();
                    break;
            }
    }
    if(!m_real_file.empty() ){
        if (!std::filesystem::exists(m_real_file)  )
        return NO_RESOURCE;

        if (!std::filesystem::readable(m_real_file)  )
            return FORBIDDEN_REQUEST;

        if (!std::filesystem::is_regular_file(m_real_file) )
            return BAD_REQUEST;
    
        return FILE_REQUEST;
    }
    return GET_REQUEST;
}

HTTP_CODE http_conn::write()
{
    if(connectET)
        while (true) {
            int temp = writev(m_sockfd, &m_iv[m_iv_idx], m_iv_count);

            if (temp < 0) {
                // TCP 写缓冲区已满，必须等待 epoll 触发下一次 EPOLLOUT 唤醒
                if (errno == EAGAIN || errno == EWOULDBLOCK) 
                    return NO_REQUEST; 
                // 真的发生了网络错误（例如客户端断开连接）
                return CLOSED_CONNECTION; 
            }

            bytes_have_send += temp;
            
            if (bytes_have_send >= bytes_to_send)
                return GET_REQUEST;
            
            if(m_iv_count == 2 && m_iv_idx == 0 && temp >= m_iv[0].iov_len ){
                int off(temp-m_iv[0].iov_len);
                m_iv_idx = 1;
                m_iv[1].iov_len -= off;
                m_iv[1].iov_base += off;
            }else{
                m_iv[m_iv_idx].iov_len -= temp;  
                m_iv[m_iv_idx].iov_base += temp;   
            }
        }     
    else{
            int temp = writev(m_sockfd, &m_iv[m_iv_idx], m_iv_count);

            if (temp < 0) {
                // TCP 写缓冲区已满，必须等待 epoll 触发下一次 EPOLLOUT 唤醒
                if (errno == EAGAIN || errno == EWOULDBLOCK) 
                    return NO_REQUEST; 
                // 真的发生了网络错误（例如客户端断开连接）
                return CLOSED_CONNECTION; 
            }

            bytes_have_send += temp;
            
            if (bytes_have_send >= bytes_to_send)
                return GET_REQUEST;
            
            if(m_iv_count == 2 && m_iv_idx == 0 && temp >= m_iv[0].iov_len ){
                int off(temp-m_iv[0].iov_len);
                m_iv_idx = 1;
                m_iv[1].iov_len -= off;
                m_iv[1].iov_base += off;
            }else{
                m_iv[m_iv_idx].iov_len -= temp;  
                m_iv[m_iv_idx].iov_base += temp;   
            }
        }     
        return NO_REQUEST;
}


bool http_conn::process_write(HTTP_CODE ret)
{
    if( ret == FILE_REQUEST){
        if (
        (add_response("HTTP/1.1 ", 200 ,' ',title[200], "\r\n" )&&    
        add_response("Content-Length: ", form[404].size(), "\r\n" ) && 
        add_response("Content-Type: ", "text/html" , "\r\n") && 
        add_response("Connection: " , "close" ,"\r\n") &&
        add_response("\r\n") 
        ) == false
        )return false;

        ++m_iv_count;
        
        m_file_size = m_iv[1].iov_len = bytes_to_send = std::filesystem::file_size(m_real_file);

        int fd = open(m_real_file.data(), O_RDONLY);
        m_iv[1].iov_base = m_file_address = (char *)mmap(0, bytes_to_send, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

    }else if(ret == BAD_REQUEST){
        if ( 
        (add_response("HTTP/1.1 ", 404 ,' ',title[404], "\r\n" ) && 
        add_response("Content-Length: ", form[404].size(), "\r\n" ) && 
        add_response("Content-Type: ", "text/plain" , "\r\n") && 
        add_response("Connection: " , "close" ,"\r\n") &&
        add_response("\r\n") &&
        add_response(form[404]) 
        )==false
        )  return false;
    }else if(ret == FORBIDDEN_REQUEST){
        if (
        (add_response("HTTP/1.1 ", 403 ,' ',title[403], "\r\n" ) && 
        add_response("Content-Length: ", form[403].size(), "\r\n" ) && 
        add_response("Content-Type: ", "text/plain" , "\r\n") && 
        add_response("Connection: " , "close" ,"\r\n") &&
        add_response("\r\n") && 
        add_response(form[403]) 
        )==false
        ) return false;
    }else if(
        (add_response("HTTP/1.1 ", 200 ,' ',title[200], "\r\n" ) && 
        add_response("\r\n") 
        ) == false
        )return false;
    
    m_iv[0].iov_base = const_cast<char*>(m_write_buf.data() );
    m_iv[0].iov_len = m_write_buf.size();
    bytes_to_send += m_iv[0].iov_len;
    return true;
}

HTTP_CODE http_conn::process(){
    
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST || read_ret == CLOSED_CONNECTION )
        return read_ret;
    
    if( read_ret != GET_REQUEST )
        m_linger = false;
    //GET_REQUEST || BAD_REQUEST || NO_RESOURCE || FORBIDDEN_REQUEST || FILE_REQUEST || INTERNAL_ERROR
    if ( !process_write(read_ret) )
        return CLOSED_CONNECTION;

    return read_ret;
}

