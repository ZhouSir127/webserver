#include "http_conn.h"

#include <algorithm>
#include <string>
#include "../consts.h"
#include <filesystem>  
#include <strings.h>

std::unordered_map<int,std::string> HttpConn::form {
    {400,"Your request has bad syntax or is inherently impossible to staisfy.\n"}, 
    {403,"You do not have permission to get file form this server.\n"},
    {404,"The requested file was not found on this server.\n"},
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
    cookie.clear();
    requestBody.clear();

    realFilePath.clear();
    token.clear();

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
            readBuffer.resize( std::min( (readBuffer.size()<<1),consts::READ_BUFFER_SIZE) );
        }
        ssize_t bytes_read = recv(fd, &readBuffer[readIdx] ,readBuffer.size()-readIdx,0);
    
        if (bytes_read < 0 ){
            if (errno == EAGAIN || errno == EINTR)//无数据可读
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
            
            ssize_t bytes_read = recv(fd, &readBuffer[readIdx], readBuffer.size()-readIdx , 0);
            if (bytes_read < 0 ){
                if (errno == EAGAIN || errno == EINTR)
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

    if ( strcasecmp(token.c_str(),"GET") == 0 )
        method = HttpMethod::GET;
    else if (  strcasecmp(token.c_str(),"POST") == 0 )
        method = HttpMethod::POST;
    else
        return HttpCode::BAD_REQUEST;

    if(!(iss >> token) )
        return HttpCode::BAD_REQUEST;
    
    size_t pos(0);

    if ( strncasecmp(token.c_str(),"http://",7) == 0){
        pos = token.find_first_of('/',7);
    
        if (pos == std::string::npos )
            return HttpCode::BAD_REQUEST;
    }else if (strncasecmp(token.c_str(),"https://",8) == 0){
        pos = token.find_first_of('/',8);
    
        if ( pos == std::string::npos )
            return HttpCode::BAD_REQUEST;
    }else if(token[0] != '/')
        return HttpCode::BAD_REQUEST;

    url = token.substr(pos);

    if(!(iss >> token) )
        return HttpCode::BAD_REQUEST;

    if (strcasecmp(token.c_str(),"HTTP/1.1") && strcasecmp(token.c_str(),"HTTP/1.0") )
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
    }else if (strncasecmp(line.c_str(), "Connection:",11) == 0){
        size_t pos = line.find_first_not_of ( " \t" , 11 );
        if(pos == std::string::npos)
            return HttpCode::BAD_REQUEST;
        if ( strncasecmp( line.c_str(),"keep-alive",10) == 0)
            isLinger = true;
    }else if (strncasecmp(line.c_str(), "Content-Length:",15) == 0){
        size_t pos = line.find_first_not_of ( " \t" , 15 );
        if(pos == std::string::npos)
            return HttpCode::BAD_REQUEST;
        contentLength = std::stoul(std::string(line.begin()+pos , line.end() ) );
    }else if(strncasecmp(line.c_str(),  "Cookie:",7) == 0){
        size_t pos = line.find_first_not_of ( " \t" , 7 );
        if(pos == std::string::npos)
            return HttpCode::BAD_REQUEST;
        cookie = std::string(line.begin()+pos , line.end() );
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
                
                return doRequest();
            }
            
            return HttpCode::NO_REQUEST;
        }else{
            ret = parseLine();
            if (ret != HttpCode::GET_REQUEST )
                return ret;
            
            if(checkState == CheckState::CHECK_STATE_REQUESTLINE ){
                ret = parseRequestLine();
                if ( ret != HttpCode::GET_REQUEST)
                    return ret;
            }else if (parseHeaders() == HttpCode::BAD_REQUEST)
                return HttpCode::BAD_REQUEST;
        }
    return HttpCode::BAD_REQUEST;
}

HttpCode HttpConn::doRequest()
{
    router.route(this);
    
    if(!realFilePath.empty() ){
        if (!std::filesystem::exists(realFilePath)  ){
            LOG_ERROR("File request Error: NO_RESOURCE (404) for path: %s", realFilePath.c_str());
            return HttpCode::NO_RESOURCE;
        }
        if (access(realFilePath.c_str(), R_OK) != 0  ){
            LOG_ERROR("File request Error: FORBIDDEN_REQUEST (403) for path: %s", realFilePath.c_str());
            return HttpCode::FORBIDDEN_REQUEST;
        }
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
            ssize_t temp = writev(fd, &ioVectors[ioVectorIdx], ioVectorCount - ioVectorIdx);

            if (temp < 0) {
                // TCP 写缓冲区已满，必须等待 epoll 触发下一次 EPOLLOUT 唤醒
                if (errno == EAGAIN || errno == EINTR) 
                    return HttpCode::NO_REQUEST; 
                // 真的发生了网络错误（例如客户端断开连接）
                return HttpCode::CLOSED_CONNECTION; 
            }

            bytesHaveSent += temp;
            
            if (bytesHaveSent >= bytesToSend)
                return HttpCode::GET_REQUEST;
            
            if(ioVectorCount == 2 && ioVectorIdx == 0 && static_cast<size_t>(temp) >= ioVectors[0].iov_len  ){
                ssize_t off(temp-ioVectors[0].iov_len);
                ioVectorIdx = 1;
                ioVectors[1].iov_len -= off;
                ioVectors[1].iov_base = static_cast<char*>(ioVectors[1].iov_base) + off;
            }else{
                ioVectors[ioVectorIdx].iov_len -= temp;  
                ioVectors[ioVectorIdx].iov_base = static_cast<char*>(ioVectors[ioVectorIdx].iov_base) + temp;   
            }
        }     
    else{
            ssize_t temp = writev(fd, &ioVectors[ioVectorIdx], ioVectorCount - ioVectorIdx);

            if (temp < 0) {
                // TCP 写缓冲区已满，必须等待 epoll 触发下一次 EPOLLOUT 唤醒
                if (errno == EAGAIN || errno == EINTR) 
                    return HttpCode::NO_REQUEST; 
                // 真的发生了网络错误（例如客户端断开连接）
                return HttpCode::CLOSED_CONNECTION; 
            }

            bytesHaveSent += temp;
            
            if (bytesHaveSent >= bytesToSend)
                return HttpCode::GET_REQUEST;
            
            if(ioVectorCount == 2 && ioVectorIdx == 0 && static_cast<size_t>(temp) >= ioVectors[0].iov_len ){
                ssize_t off(temp-ioVectors[0].iov_len);
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
        addResponse("Cache-Control: no-store, no-cache, must-revalidate\r\n") &&
        addResponse("Content-Length: ",fileSize = ioVectors[1].iov_len = bytesToSend = std::filesystem::file_size(realFilePath), "\r\n" ) && 
        addResponse("Content-Type: ", "text/html" , "\r\n") && 
        addResponse("Connection: " , isLinger ? "keep-alive" : "close" ,"\r\n") &&
        ( !token.empty() ? addResponse("Set-Cookie: token=" + token + "; Path=/; Max-Age=3600; HttpOnly\r\n"):true ) &&
        addResponse("\r\n") 
        ) == false
        )return false;

        ioVectorCount = 2;
        int fd = open(realFilePath.data(), O_RDONLY);
        ioVectors[1].iov_base = fileAddress = static_cast<char*>(mmap(0, bytesToSend, PROT_READ, MAP_PRIVATE, fd, 0));
        close(fd);

    }else if(ret == HttpCode::BAD_REQUEST){
        if ( 
        (addResponse("HTTP/1.1 ", 400 ,' ',title[400], "\r\n" ) && 
        addResponse("Content-Length: ", form[400].size(), "\r\n" ) && 
        addResponse("Content-Type: ", "text/plain" , "\r\n") && 
        addResponse("Connection: " , isLinger ? "keep-alive" : "close" ,"\r\n") &&
        ( !token.empty() ? addResponse("Set-Cookie: token=" + token + "; Path=/; Max-Age=3600; HttpOnly\r\n"):true ) &&
        addResponse("\r\n") &&
        addResponse(form[400]) 
        )==false
        )  return false;
    }else if(ret == HttpCode::FORBIDDEN_REQUEST){
        if (
        (addResponse("HTTP/1.1 ", 403 ,' ',title[403], "\r\n" ) && 
        addResponse("Content-Length: ", form[403].size(), "\r\n" ) && 
        addResponse("Content-Type: ", "text/plain" , "\r\n") && 
        addResponse("Connection: " , isLinger ? "keep-alive" : "close" ,"\r\n") &&
        ( !token.empty() ? addResponse("Set-Cookie: token=" + token + "; Path=/; Max-Age=3600; HttpOnly\r\n"):true )&&
        addResponse("\r\n") && 
        addResponse(form[403]) 
        )==false
        ) return false;
    }else if(ret == HttpCode::NO_RESOURCE) { // 这是真正的 404 处理
        if ( 
        (addResponse("HTTP/1.1 ", 404 ,' ',title[404], "\r\n" ) && 
        addResponse("Content-Length: ", form[404].size(), "\r\n" ) && 
        addResponse("Content-Type: ", "text/plain" , "\r\n") && 
        addResponse("Connection: " , isLinger ? "keep-alive" : "close" ,"\r\n") &&
        ( !token.empty() ? addResponse("Set-Cookie: token=" + token + "; Path=/; Max-Age=3600; HttpOnly\r\n"):true )&&
        addResponse("\r\n") &&
        addResponse(form[404]) 
        )==false
        ) return false;
    }else if(
        (addResponse("HTTP/1.1 ", 200 ,' ',title[200], "\r\n" ) && addResponse("Content-Length: 0\r\n")&&
        ( !token.empty() ? addResponse("Set-Cookie: token=" + token + "; Path=/; Max-Age=3600; HttpOnly\r\n"):true )&&
        addResponse("\r\n") 
        ) == false
        )return false;
    
    ioVectors[0].iov_base = writeBuffer.data();
    ioVectors[0].iov_len = writeBuffer.size();
    bytesToSend += ioVectors[0].iov_len;
    return true;
}

HttpCode HttpConn::process(){
    
    HttpCode read_ret = processRead();
    if (read_ret == HttpCode::NO_REQUEST || read_ret == HttpCode::CLOSED_CONNECTION )
        return read_ret;
    
    if( !(read_ret == HttpCode::GET_REQUEST || read_ret == HttpCode::FILE_REQUEST ) )
        isLinger = false;
    //GET_REQUEST || BAD_REQUEST || NO_RESOURCE || FORBIDDEN_REQUEST || FILE_REQUEST || INTERNAL_ERROR
    if ( !processWrite(read_ret) )
        return HttpCode::CLOSED_CONNECTION;

    return read_ret;
}

