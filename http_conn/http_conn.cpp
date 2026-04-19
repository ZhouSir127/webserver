#include "http_conn.h"

#include <mysql/mysql.h> 
#include <algorithm>
#include <memory>
#include <cstdlib>
#include <string>
#include <sstream>
#include <filesystem>  
#include "../consts.h"

std::shared_ptr<mavsdk::Mavsdk> mavsdkPtr = nullptr;
std::shared_ptr<mavsdk::System> drone = nullptr;
std::shared_ptr<mavsdk::Action> action = nullptr;
std::shared_ptr<mavsdk::Telemetry> telemetry = nullptr;

std::unordered_map<int,std::string> HttpConn::form {
    {400,"Your request has bad syntax or is inherently impossible to staisfy.\n"}, 
    {403,"You do not have permission to get file form this server.\n"},
    {404,"The requested file was not found on this server.\n"},
    //{500,"There was an unusual problem serving the request file.\n"}
};    
std::unordered_map<int,std::string> HttpConn::title {
    {200,"OK"},
    {400,"Bad Request"}, 
    {403,"Forbidden"},
    {404,"Not Found"},
};    

void HttpConn::init()
{
    readBuffer.resize(1024);
    readIdx = 0;
    checkedIdx = 0;
    startIdx = 0;

    checkState = CheckState::CHECK_STATE_REQUESTLINE;
    line.clear();
    method = HttpMethod::GET;
    url.clear();
    isLinger = false;
    contentLength = 0;
    requestBody.clear();

    realFilePath.clear();
    
    writeBuffer.clear();
    bytesToSend = 0;
    bytesHaveSent = 0;

    ioVectorCount = 1;
    ioVectorIdx = 0;

    if (fileAddress) {
        munmap(fileAddress, fileSize);
        fileAddress = nullptr;
    }
    fileSize = 0;
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
HttpCode HttpConn::parseLine()
{
    while (checkedIdx < readIdx ){
        if ( readBuffer[checkedIdx] == '\r')
        {
            if ( checkedIdx + 1 == readIdx )
                return HttpCode::NO_REQUEST;
            else if ( readBuffer[checkedIdx+1] == '\n'){

                line = std::string(readBuffer.begin() + startIdx , readBuffer.begin() + checkedIdx);
                
                checkedIdx += 2;
                startIdx = checkedIdx;

                return HttpCode::GET_REQUEST;
            }
            return HttpCode::BAD_REQUEST;//'\r后非'\n'
        }
        ++checkedIdx;
    }
    
    return HttpCode::NO_REQUEST;//没读到'/r'
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
HttpCode HttpConn::readOnce()
{
    //LT读取数据
    if (!isConnectEt){
        if(readBuffer.size()==readIdx){
            if(readBuffer.size() == consts::READ_BUFFER_SIZE)
                return HttpCode::BAD_REQUEST;
            readBuffer.resize( std::min( (readBuffer.size()<<1),(size_t)consts::READ_BUFFER_SIZE) );
        }
        int bytes_read = recv(socketFd, &readBuffer[readIdx] ,readBuffer.size()-readIdx,0);
    
        if (bytes_read < 0 ){
            if (errno == EAGAIN || errno == EWOULDBLOCK)//无数据可读
                return HttpCode::NO_REQUEST;
            return HttpCode::BAD_REQUEST;//读故障
        }else if (bytes_read == 0)//对方正常关闭了连接
            return HttpCode::CLOSED_CONNECTION;
        
        readIdx += bytes_read;
    }else
        while (true)
        {    
            if(readBuffer.size()==readIdx)
                readBuffer.resize(readBuffer.size()<<1 );
            
            int bytes_read = recv(socketFd, &readBuffer[readIdx], readBuffer.size()-readIdx , 0);
            if (bytes_read < 0 ){
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return HttpCode::BAD_REQUEST;
            }else if (bytes_read == 0)
                return HttpCode::CLOSED_CONNECTION;
            
            readIdx += bytes_read;
        }
    return HttpCode:: GET_REQUEST;
}

//解析http请求行，获得请求方法，目标url及http版本号
HttpCode HttpConn::parseRequestLine()
{
    std::istringstream iss(line);
    std::string token;

    if(!(iss >> token) )
        return HttpCode::BAD_REQUEST;

    if ( token.compare("GET") == 0 )
        method = HttpMethod::GET;
    else if (  token.compare("POST") == 0 )
        method = HttpMethod::POST;
    else
        return HttpCode::BAD_REQUEST;

    if(!(iss >> token) )
        return HttpCode::BAD_REQUEST;
    
    size_t pos(0);

    if ( token.compare(0 , 7 ,"http://") == 0){
        pos = token.find_first_of('/',7);
    
        if (pos == std::string::npos )
            return HttpCode::BAD_REQUEST;
    }else if (token.compare(0, 8 ,"https://") == 0){
        pos = token.find_first_of('/',8);
    
        if ( pos == std::string::npos )
            return HttpCode::BAD_REQUEST;
    }else if(token[0] != '/')
        return HttpCode::BAD_REQUEST;

    url = token;

    if(!(iss >> token) )
        return HttpCode::BAD_REQUEST;

    if (token.compare(pos , 0 ,"HTTP/1.1") != 0)
        return HttpCode::BAD_REQUEST;    

    checkState = CheckState::CHECK_STATE_HEADER;
    return HttpCode::GET_REQUEST;
}

//解析http请求的一个头部信息
HttpCode HttpConn::parseHeaders()
{
    if ( line.empty() )//请求头最后的空行
    {
        checkState = CheckState::CHECK_STATE_CONTENT;       
        return HttpCode::GET_REQUEST;
    }else if (line.compare(0, 11 ,"Connection:") == 0){
        size_t pos = line.find_first_not_of ( " \t" , 11 );
        if(pos == std::string::npos)
            return HttpCode::BAD_REQUEST;
        if ( line.compare(pos, 10 , "keep-alive") == 0)
            isLinger = true;
    }else if (line.compare(0 , 15 , "Content-length:") == 0){
        size_t pos = line.find_first_not_of ( " \t" , 15 );
        if(pos == std::string::npos)
            return HttpCode::BAD_REQUEST;

        contentLength = std::stoul(std::string(line.begin()+pos , line.end() ) );
    }

    return HttpCode::GET_REQUEST;
}

HttpCode HttpConn::processRead()
{
    HttpCode ret = readOnce();
    if ( ret != HttpCode:: GET_REQUEST )//对方关闭了连接或读故障或缓冲大小上限或
        return ret;
    
    while(true)
        if(checkState == CheckState::CHECK_STATE_CONTENT){
            if (contentLength == 0) 
                return doRequest();
            else if (readIdx - checkedIdx >= contentLength )
            {
                //m_read_buf[m_checked,m_read_idx)已经读到的但尚未check
                //text[0:content_length)为请求体内容
                checkedIdx = startIdx + contentLength ;
                //POST请求中最后为输入的用户名和密码
                requestBody = std::string(readBuffer.begin() + startIdx ,readBuffer.begin() + checkedIdx);
                startIdx = checkedIdx = readIdx = 0;
                
                return doRequest();
            }
            
            return HttpCode::NO_REQUEST;
        }else{
            HttpCode ret = parseLine();
            if (ret != HttpCode::GET_REQUEST )
                return ret;
            
            if(checkState == CheckState::CHECK_STATE_REQUESTLINE ){
                HttpCode ret = parseRequestLine();
                if ( ret != HttpCode::GET_REQUEST)
                    return ret;
            }else if (parseHeaders() == HttpCode::BAD_REQUEST)
                return HttpCode::BAD_REQUEST;
        }
    return HttpCode::BAD_REQUEST;
}

HttpCode HttpConn::doRequest()
{
    if(method == HttpMethod::POST){
        std::string name , password;
        std::istringstream iss(requestBody);
        
        if( !(iss >> name) )
            return HttpCode::BAD_REQUEST;
        if( !(iss >> password) )
            return HttpCode::BAD_REQUEST;
    
        if (url[1] == 0){    //注册
            if (users.exists(name) ==false ){
                std::string sql_insert = "INSERT INTO user(username, passwd) VALUES('" 
                        + name 
                        + "', '" 
                        + password 
                        + "')";

                MYSQL *mysql=nullptr;
                connectionRAII mysqlcon(mysql, &connPool);
                
                if (mysql_query(mysql, sql_insert.data() ) == 0){
                    realFilePath = std::string(root + "/log.html");
                    users.add(name,password);
                }else
                    realFilePath = std::string(root + "/registerError.html");
            }else
                realFilePath = std::string(root + "/registerError.html");
    
        }else if ( users.check(name,password) )
            realFilePath = std::string(root + "/index.html");
        else
            realFilePath = std::string(root + "/logError.html");    
    }else{
        if(url.size() == 1)
            realFilePath = std::string(root + "/log.html" );
        else
            switch (url[1]){
                case 0:
                    realFilePath = std::string(root + "/register.html" );
                    break;
                case 1:
                    if (!mavsdkPtr) {
                        mavsdk::Mavsdk::Configuration config(mavsdk::ComponentType::CompanionComputer);
                        mavsdkPtr = std::make_shared<mavsdk::Mavsdk>(config);
                    }
                    
                    // 继续尝试连接飞控
                    mavsdkPtr->add_any_connection("udp://:14540");
                    
                    // 简易处理：等待飞控连接
                    usleep(100000); 
                    if (!mavsdkPtr) {
                        mavsdk::Mavsdk::Configuration config(mavsdk::ComponentType::CompanionComputer);
                        mavsdkPtr = std::make_shared<mavsdk::Mavsdk>(config);
                        // 连接动作只在第一次进行
                        mavsdkPtr->add_any_connection("udp://:14540");
                    }
                    break;
                case 2:// /2 -> /disconnect
                    action = nullptr;
                    telemetry = nullptr;
                    drone = nullptr;
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
    if(!realFilePath.empty() ){
        if (!std::filesystem::exists(realFilePath)  )
            return HttpCode::NO_RESOURCE;

        if (access(realFilePath.c_str(), R_OK) != 0  )
            return HttpCode::FORBIDDEN_REQUEST;

        if (!std::filesystem::is_regular_file(realFilePath) )
            return HttpCode::BAD_REQUEST;
    
        return HttpCode::FILE_REQUEST;
    }
    return HttpCode::GET_REQUEST;
}

HttpCode HttpConn::write()
{
    if(isConnectEt)
        while (true) {
            ssize_t temp = writev(socketFd, &ioVectors[ioVectorIdx], ioVectorCount);

            if (temp < 0) {
                // TCP 写缓冲区已满，必须等待 epoll 触发下一次 EPOLLOUT 唤醒
                if (errno == EAGAIN || errno == EWOULDBLOCK) 
                    return HttpCode::NO_REQUEST; 
                // 真的发生了网络错误（例如客户端断开连接）
                return HttpCode::CLOSED_CONNECTION; 
            }

            bytesHaveSent += temp;
            
            if (bytesHaveSent >= bytesToSend)
                return HttpCode::GET_REQUEST;
            
            if(ioVectorCount == 2 && ioVectorIdx == 0 && static_cast<size_t>(temp) >= ioVectors[0].iov_len ){
                int off(temp-ioVectors[0].iov_len);
                ioVectorIdx = 1;
                ioVectors[1].iov_len -= off;
                ioVectors[1].iov_base = static_cast<char*>(ioVectors[1].iov_base) + off;
            }else{
                ioVectors[ioVectorIdx].iov_len -= temp;  
                ioVectors[ioVectorIdx].iov_base = static_cast<char*>(ioVectors[ioVectorIdx].iov_base) + temp;   
            }
        }     
    else{
            int temp = writev(socketFd, &ioVectors[ioVectorIdx], ioVectorCount);

            if (temp < 0) {
                // TCP 写缓冲区已满，必须等待 epoll 触发下一次 EPOLLOUT 唤醒
                if (errno == EAGAIN || errno == EWOULDBLOCK) 
                    return HttpCode::NO_REQUEST; 
                // 真的发生了网络错误（例如客户端断开连接）
                return HttpCode::CLOSED_CONNECTION; 
            }

            bytesHaveSent += temp;
            
            if (bytesHaveSent >= bytesToSend)
                return HttpCode::GET_REQUEST;
            
            if(ioVectorCount == 2 && ioVectorIdx == 0 && static_cast<size_t>(temp) >= ioVectors[0].iov_len ){
                int off(temp-ioVectors[0].iov_len);
                ioVectorIdx = 1;
                ioVectors[1].iov_len -= off;
                ioVectors[1].iov_base = static_cast<char*>(ioVectors[1].iov_base) + off;
            }else{
                ioVectors[ioVectorIdx].iov_len -= temp;  
                ioVectors[ioVectorIdx].iov_base = static_cast<char*>(ioVectors[ioVectorIdx].iov_base) + temp;   
            }
        }     
        return HttpCode::NO_REQUEST;
}


bool HttpConn::processWrite(HttpCode ret)
{
    if( ret == HttpCode::FILE_REQUEST){
        if (
        (addResponse("HTTP/1.1 ", 200 ,' ',title[200], "\r\n" )&&    
        addResponse("Content-Length: ",fileSize = ioVectors[1].iov_len = bytesToSend = std::filesystem::file_size(realFilePath), "\r\n" ) && 
        addResponse("Content-Type: ", "text/html" , "\r\n") && 
        addResponse("Connection: " , "close" ,"\r\n") &&
        addResponse("\r\n") 
        ) == false
        )return false;

        int fd = open(realFilePath.data(), O_RDONLY);
        ioVectors[1].iov_base = fileAddress = (char *)mmap(0, bytesToSend, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

    }else if(ret == HttpCode::BAD_REQUEST){
        if ( 
        (addResponse("HTTP/1.1 ", 400 ,' ',title[400], "\r\n" ) && 
        addResponse("Content-Length: ", form[400].size(), "\r\n" ) && 
        addResponse("Content-Type: ", "text/plain" , "\r\n") && 
        addResponse("Connection: " , "close" ,"\r\n") &&
        addResponse("\r\n") &&
        addResponse(form[400]) 
        )==false
        )  return false;
    }else if(ret == HttpCode::FORBIDDEN_REQUEST){
        if (
        (addResponse("HTTP/1.1 ", 403 ,' ',title[403], "\r\n" ) && 
        addResponse("Content-Length: ", form[403].size(), "\r\n" ) && 
        addResponse("Content-Type: ", "text/plain" , "\r\n") && 
        addResponse("Connection: " , "close" ,"\r\n") &&
        addResponse("\r\n") && 
        addResponse(form[403]) 
        )==false
        ) return false;
    }else if(ret == HttpCode::NO_RESOURCE) { // 这是真正的 404 处理
        if ( 
        (addResponse("HTTP/1.1 ", 404 ,' ',title[404], "\r\n" ) && 
        addResponse("Content-Length: ", form[404].size(), "\r\n" ) && 
        addResponse("Content-Type: ", "text/plain" , "\r\n") && 
        addResponse("Connection: " , "close" ,"\r\n") &&
        addResponse("\r\n") &&
        addResponse(form[404]) 
        )==false
        ) return false;
    }else if(
        (addResponse("HTTP/1.1 ", 200 ,' ',title[200], "\r\n" ) && 
        addResponse("\r\n") 
        ) == false
        )return false;
    
    ioVectors[0].iov_base = const_cast<char*>(writeBuffer.data() );
    ioVectors[0].iov_len = writeBuffer.size();
    bytesToSend += ioVectors[0].iov_len;
    return true;
}

HttpCode HttpConn::process(){
    
    HttpCode read_ret = processRead();
    if (read_ret == HttpCode::NO_REQUEST || read_ret == HttpCode::CLOSED_CONNECTION )
        return read_ret;
    
    if( read_ret != HttpCode::GET_REQUEST )
        isLinger = false;
    //GET_REQUEST || BAD_REQUEST || NO_RESOURCE || FORBIDDEN_REQUEST || FILE_REQUEST || INTERNAL_ERROR
    if ( !processWrite(read_ret) )
        return HttpCode::CLOSED_CONNECTION;

    return read_ret;
}

