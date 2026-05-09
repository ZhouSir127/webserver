#include "http_conn.h"

#include <algorithm>
#include <string>
#include "../consts.h"
#include <filesystem>  
#include <strings.h>

std::unordered_map<int,std::string> Message::form {
    {400,"Your request has bad syntax or is inherently impossible to staisfy.\n"}, 
    {403,"You do not have permission to get file form this server.\n"},
    {404,"The requested file was not found on this server.\n"},
    {500,"The server encountered an internal error and was unable to complete your request.\n"}
};    
std::unordered_map<int,std::string> Message::title {
    {200,"OK"},
    {400,"Bad Request"}, 
    {403,"Forbidden"},
    {404,"Not Found"},
    {500,"Internal Server Error"}
};    

void Message::parseLine()
{
    while (checkedIdx != endIdx ){
        size_t next = (checkedIdx + 1)%consts::READ_BUFFER_SIZE; 
        if ( readBuffer[checkedIdx] == '\r'){
            if ( next == endIdx ){
                status = HttpCode::NO_REQUEST;
                return;
            }else if ( readBuffer[next] == '\n'){
                if(startIdx <= checkedIdx)
                    line = std::string(readBuffer.begin() + startIdx , readBuffer.begin() + checkedIdx);
                else
                    line = readBuffer.substr(startIdx) + std::string(readBuffer.begin(),readBuffer.begin() + checkedIdx ) ;
                
                startIdx = checkedIdx = (next+1)%consts::READ_BUFFER_SIZE;
                
                status = HttpCode::GET_REQUEST;
                return;
            }else{
                status = HttpCode::BAD_REQUEST;
                return;
            }
        }
        checkedIdx = next;
    }
    status = HttpCode::NO_REQUEST;
}

//解析http请求行，获得请求方法，目标url及http版本号
void Message::parseRequestLine()
{
    std::istringstream iss(line);
    std::string token;

    if(!(iss >> token) ){
        status = HttpCode::BAD_REQUEST;
        return;
    }
    if ( strcasecmp(token.c_str(),"GET") == 0 )
        method = HttpMethod::GET;
    else if (  strcasecmp(token.c_str(),"POST") == 0 )
        method = HttpMethod::POST;
    else{
        status = HttpCode::BAD_REQUEST;
        return;
    }


    if(!(iss >> token) ){
        status = HttpCode::BAD_REQUEST;
        return;
    }
    size_t pos(0);
    
    if ( strncasecmp(token.c_str(),consts::SCHEME_HTTP.data(),consts::SCHEME_HTTP.size() ) == 0)
        pos = token.find_first_of('/',consts::SCHEME_HTTP.size() );
    else if (strncasecmp(token.c_str(),consts::SCHEME_HTTPS.data(),consts::SCHEME_HTTPS.size() ) == 0)
        pos = token.find_first_of('/',consts::SCHEME_HTTPS.size() );
    
    if( pos == std::string::npos || token[pos] != '/'){
        status = HttpCode::BAD_REQUEST;
        return;
    }else
        url = token.substr(pos);


    if(!(iss >> token) ){
        status = HttpCode::BAD_REQUEST;
        return;
    }

    if (strcasecmp(token.c_str(),"HTTP/1.1") >0 && strcasecmp(token.c_str(),"HTTP/1.0") >0 ){
        status = HttpCode::BAD_REQUEST;
        return;
    }

    checkState = CheckState::CHECK_STATE_HEADER;
    status = HttpCode::GET_REQUEST;
}

void Message::parseHeaders()
{
    if ( line.empty() )
        checkState = CheckState::CHECK_STATE_CONTENT;       
    else if (strncasecmp(line.c_str(), consts::HEADER_CONNECTION.data(),consts::HEADER_CONNECTION.size() ) == 0){
        size_t pos = line.find_first_not_of ( " \t" , consts::HEADER_CONNECTION.size() );
        if(pos == std::string::npos){
            status = HttpCode::BAD_REQUEST;
            return;
        }
        if ( strncasecmp( line.c_str()+pos,consts::VALUE_CLOSE.data(),consts::VALUE_CLOSE.size() ) == 0)
            isLinger = false;
    }else if (strncasecmp(line.c_str(),consts::HEADER_CONTENT_LENGTH.data(),consts::HEADER_CONTENT_LENGTH.size() ) == 0){
        size_t pos = line.find_first_not_of ( " \t" , consts::HEADER_CONTENT_LENGTH.size() );
        if(pos == std::string::npos){
            status = HttpCode::BAD_REQUEST;
            return;
        }
        contentLength = std::stoul(line.substr(pos));
    }else if(strncasecmp(line.c_str(), consts::HEADER_COOKIE.data(),consts::HEADER_COOKIE.size() ) == 0){
        size_t pos = line.find_first_not_of ( " \t" , consts::HEADER_COOKIE.size() );
        if(pos == std::string::npos){
            status = HttpCode::BAD_REQUEST;
            return;
        }
        cookie = line.substr(pos);
    }       
    status = HttpCode::GET_REQUEST;
}

void Message::parse()
{
    while(true)
        if(checkState == CheckState::CHECK_STATE_CONTENT){
            if (contentLength == 0) 
                break;
            else if ( (endIdx+ consts::READ_BUFFER_SIZE - startIdx)%consts::READ_BUFFER_SIZE >= contentLength ){
                if(startIdx + contentLength <= consts::READ_BUFFER_SIZE ){
                    requestBody = readBuffer.substr(startIdx ,contentLength);
                    startIdx = (startIdx+contentLength)%consts::READ_BUFFER_SIZE;
                }else{
                    size_t newStartIdx= contentLength - (consts::READ_BUFFER_SIZE-startIdx);
                    requestBody = readBuffer.substr(startIdx) + std::string(readBuffer.begin(),readBuffer.begin() + newStartIdx );
                    startIdx = newStartIdx;
                }    
                break;
            }else{
                status = HttpCode::NO_REQUEST;
                return;
            }
        }else{
            parseLine();
            if (status != HttpCode::GET_REQUEST )//NO BAD GET
                return;
            
            if(checkState == CheckState::CHECK_STATE_REQUESTLINE ){
                if(line.empty() )
                    continue;
                parseRequestLine();//BAD GET
                if ( status != HttpCode::GET_REQUEST)
                    return;
            }else if(checkState == CheckState::CHECK_STATE_HEADER){
                parseHeaders();
                if ( status != HttpCode::GET_REQUEST) //BAD GET
                    return;
            }
        }
    status = HttpCode::GET_REQUEST;
}

bool HttpConn::read(){//false为关闭连接，true为接下来写mess
    if(isConnectEt == false){
        if( (endIdx + 1)%consts::READ_BUFFER_SIZE == startIdx){
            LOG_WARN("Read buffer overflow (LT). Malicious client? fd: ", fd);
            return true;
        }
        
        struct iovec iov[2];
        int iovCount = 1;

        if(endIdx < startIdx ){
            iov[0].iov_base = &readBuffer[endIdx];
            iov[0].iov_len = startIdx - endIdx -1;
        }else{
            iovCount = 2;
            iov[0].iov_base = &readBuffer[endIdx];
            iov[1].iov_base = &readBuffer[0]; 
            if(startIdx == 0){
                iov[0].iov_len = consts::READ_BUFFER_SIZE - endIdx - 1;
                iov[1].iov_len = 0;
            }else{
                iov[0].iov_len = consts::READ_BUFFER_SIZE - endIdx;
                iov[1].iov_len = startIdx-1;
            }
        }    

        int bytesRead = readv(fd,iov,iovCount);
        if(bytesRead > 0){
            std::lock_guard<std::mutex>Lock(lock);
            endIdx = (endIdx+bytesRead) % consts::READ_BUFFER_SIZE;
            while(true){
                size_t oldStartIdx = startIdx;
                Message mess = Message(readBuffer,startIdx,endIdx);
                
                if(mess.getStatus() == HttpCode::GET_REQUEST){
                    mess.prepare(router);
                    messQueue.emplace(std::move(mess) );
                }else{
                    if( mess.getStatus() == HttpCode::BAD_REQUEST){
                        mess.prepare(router);
                        messQueue.emplace(std::move(mess) );
                        startIdx = endIdx = 0;
                    }else if(mess.getStatus() == HttpCode::NO_REQUEST)
                        startIdx = oldStartIdx;

                    break;
                }
            }
        }else if (bytesRead == 0)
            return false;   
        else if(errno != EAGAIN && errno != EINTR)
            return false;
    }else
        while (true){    
            if((endIdx + 1)%consts::READ_BUFFER_SIZE == startIdx ){
                LOG_WARN("Read buffer overflow (ET). Malicious client? fd: ", fd);
                return true;
            }
     
            struct iovec iov[2];
            int iovCount = 1;

            if(endIdx < startIdx){
                iov[0].iov_base = &readBuffer[endIdx];
                iov[0].iov_len = startIdx - endIdx-1;
            }else{
                iovCount = 2;
                iov[0].iov_base = &readBuffer[endIdx];
                iov[1].iov_base = &readBuffer[0]; 
                if(startIdx == 0){
                    iov[0].iov_len = consts::READ_BUFFER_SIZE - endIdx - 1;
                    iov[1].iov_len = 0;
                }else{
                    iov[0].iov_len = consts::READ_BUFFER_SIZE - endIdx;
                    iov[1].iov_len = startIdx-1;
                }
            }
            int bytesRead = readv(fd,iov,iovCount);
            if (bytesRead > 0 ){
                std::lock_guard<std::mutex>Lock(lock);
                endIdx = (endIdx + bytesRead)%consts::READ_BUFFER_SIZE;    
                while(true){
                    size_t oldStartIdx = startIdx;
                    Message mess = Message(readBuffer,startIdx,endIdx);
                    
                    if(mess.getStatus() == HttpCode::GET_REQUEST){
                        mess.prepare(router);
                        messQueue.emplace(std::move(mess) );
                    }else{
                        if( mess.getStatus() == HttpCode::BAD_REQUEST){
                            mess.prepare(router);
                            messQueue.emplace(std::move(mess) );
                            startIdx = endIdx = 0;
                        }else if(mess.getStatus() == HttpCode::NO_REQUEST)
                            startIdx = oldStartIdx;

                        break;
                    }
                }   
            }else if (bytesRead == 0)
                return false;   
            else if(errno != EAGAIN && errno != EINTR)
                return false;
        }
    EpollManager::getInstance().modify(httpChannel.get(), EPOLLIN | EPOLLPRI | EPOLLOUT | EPOLLRDHUP | EPOLLONESHOT | (isConnectEt ? EPOLLET : static_cast<uint32_t>(0)) );
    return true;
}

void Message::prepare(Router&router){
    //GET,(NO),BAD
    if(status == HttpCode::GET_REQUEST){
        router.route(this);
        prepareFile();
    }
    if( status == HttpCode::BAD_REQUEST || status == HttpCode::FORBIDDEN_REQUEST || status == HttpCode::NO_RESOURCE || status == HttpCode::INTERNAL_ERROR){
        isLinger = false;
        LOG_WARN("Bad HTTP request syntax" );
    }
    if ( prepareHeaders(status) == false )
        status = HttpCode::CLOSED_CONNECTION; 
}

void Message::prepareFile()
{   
    if(realFilePath.empty() == false ){
        if (std::filesystem::is_regular_file(realFilePath) == false  ){
            LOG_WARN("404 Not Found requested. path: ", realFilePath);
            status = HttpCode::NO_RESOURCE;
            return;
        }
        if (access(realFilePath.c_str(), R_OK) != 0  ){
            LOG_ERROR("File permission denied (403). WebServer lacks R_OK for path: ", realFilePath);
            status = HttpCode::FORBIDDEN_REQUEST;
            return;
        }
            
        size_t fileSize = std::filesystem::file_size(realFilePath);
        if(fileSize > 0){
            ioVectorCount = 2;
            ioVectors[1].iov_len = fileSize;
            int fd = open(realFilePath.data(), O_RDONLY);
            ioVectors[1].iov_base = fileAddress = static_cast<char*>(mmap(0, fileSize,PROT_READ, MAP_PRIVATE, fd, 0));
            close(fd);
            if (fileAddress == MAP_FAILED) {
                LOG_ERROR("mmap failed for file: ", realFilePath, " errno: ", errno);
                fileAddress = nullptr;
                status = HttpCode::INTERNAL_ERROR;
                return;
            }
            ioVectors[1].iov_len = fileSize = std::filesystem::file_size(realFilePath);
            
            status = HttpCode::FILE_REQUEST;
            return;
        }
    }
    status = HttpCode::GET_REQUEST;
}

bool Message::prepareHeaders(HttpCode ret)
{
    if(ret == HttpCode::GET_REQUEST){
        if(
        (addResponse("HTTP/1.1 ", 200 ,' ',title[200], "\r\n" ) && addResponse("Content-Length: 0\r\n")&&
        ( !token.empty() ? addResponse("Set-Cookie: token=" + token + "; Path=/; Max-Age=3600; HttpOnly\r\n"):true )&&
        addResponse("\r\n") 
        ) == false
        )return false;
    }else if( ret == HttpCode::FILE_REQUEST){
        if (
        (addResponse("HTTP/1.1 ", 200 ,' ',title[200], "\r\n" )&&    
        addResponse("Cache-Control: no-store, no-cache, must-revalidate\r\n") &&
        addResponse("Content-Length: ",fileSize, "\r\n" ) && 
        addResponse("Content-Type: ", "text/html" , "\r\n") && 
        addResponse("Connection: " , isLinger ? "keep-alive" : "close" ,"\r\n") &&
        ( token.empty()==false ? addResponse("Set-Cookie: token=" + token + "; Path=/; Max-Age=3600; HttpOnly\r\n"):true ) &&
        addResponse("\r\n") 
        ) == false
        )return false;    
    }else if(ret == HttpCode::BAD_REQUEST){
        if ( 
        (addResponse("HTTP/1.1 ", 400 ,' ',title[400], "\r\n" ) && 
        addResponse("Content-Length: ", form[400].size(), "\r\n" ) && 
        addResponse("Content-Type: ", "text/plain" , "\r\n") && 
        addResponse("Connection: " , isLinger ? "keep-alive" : "close" ,"\r\n") &&
        ( token.empty() == false ? addResponse("Set-Cookie: token=" + token + "; Path=/; Max-Age=3600; HttpOnly\r\n"):true ) &&
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
        ( token.empty() ==false ? addResponse("Set-Cookie: token=" + token + "; Path=/; Max-Age=3600; HttpOnly\r\n"):true )&&
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
        ( token.empty() ==false ? addResponse("Set-Cookie: token=" + token + "; Path=/; Max-Age=3600; HttpOnly\r\n"):true )&&
        addResponse("\r\n") &&
        addResponse(form[404]) 
        )==false
        ) return false;
    }else if(ret == HttpCode::INTERNAL_ERROR){
        if ( 
        (addResponse("HTTP/1.1 ", 500 ,' ',title[500], "\r\n" ) && 
        addResponse("Content-Length: ", form[500].size(), "\r\n" ) && 
        addResponse("Content-Type: ", "text/plain" , "\r\n") && 
        addResponse("Connection: " , isLinger ? "keep-alive" : "close" ,"\r\n") &&
        ( token.empty() == false ? addResponse("Set-Cookie: token=" + token + "; Path=/; Max-Age=3600; HttpOnly\r\n"):true )&&
        addResponse("\r\n") &&
        addResponse(form[500]) 
        )==false
        ) return false;
    }
    
    ioVectors[0].iov_base = writeBuffer.data();
    ioVectors[0].iov_len = writeBuffer.size();
    return true;
}


HttpCode Message::write(bool isConnectEt,int fd)
{
    if(isConnectEt)
        while (true) {
            int temp = writev(fd, ioVectors+ioVectorIdx, ioVectorCount - ioVectorIdx);

            if(temp == 0)
                return HttpCode::GET_REQUEST;
            if (temp < 0) {
                if (errno == EAGAIN || errno == EINTR) 
                    break;
                return HttpCode::CLOSED_CONNECTION;
            }
            
            if(ioVectorCount == 2 && ioVectorIdx == 0 && temp >= ioVectors[0].iov_len  ){
                temp-=ioVectors[0].iov_len;
                ioVectorIdx = 1;
            }

            ioVectors[ioVectorIdx].iov_len -= temp;  
            ioVectors[ioVectorIdx].iov_base = static_cast<char*>(ioVectors[ioVectorIdx].iov_base) + temp;   
        }     
    else{
            int temp = writev(fd, ioVectors+ioVectorIdx, ioVectorCount - ioVectorIdx);
            if(temp == 0)
                return HttpCode::GET_REQUEST;
            if (temp < 0) {
                if (errno == EAGAIN || errno == EINTR) 
                    return HttpCode::NO_REQUEST; 
                return HttpCode::CLOSED_CONNECTION; 
            }

            if(ioVectorCount == 2 && ioVectorIdx == 0 && temp >= ioVectors[0].iov_len  ){
                temp-=ioVectors[0].iov_len;
                ioVectorIdx = 1;
            }

            ioVectors[ioVectorIdx].iov_len -= temp;  
            ioVectors[ioVectorIdx].iov_base = static_cast<char*>(ioVectors[ioVectorIdx].iov_base) + temp;   
        }     
        return HttpCode::NO_REQUEST;
}

bool HttpConn::write(){
    std::lock_guard<std::mutex>Lock(lock);
    while(messQueue.empty() == false){
        Message&mess = messQueue.front();
        if(mess.getStatus() == HttpCode::CLOSED_CONNECTION)
            return false;
        else if(mess.getStatus() == HttpCode::NO_REQUEST)
            return true;
        else{
            mess.write(isConnectEt,fd);
            HttpCode status = mess.getStatus();

            if( status == HttpCode::CLOSED_CONNECTION )
                return false;
            else if ( status == HttpCode::NO_REQUEST ){
                EpollManager::getInstance().modify(httpChannel.get(), EPOLLIN | EPOLLPRI | EPOLLOUT |EPOLLRDHUP | EPOLLONESHOT | (isConnectEt ? EPOLLET : static_cast<uint32_t>(0)) );
                return true;
            }else if(status == HttpCode::GET_REQUEST && isLinger == false)
                return false;        
        }
        messQueue.pop();
    }
    EpollManager::getInstance().modify(httpChannel.get(), EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLONESHOT | (isConnectEt ? EPOLLET : static_cast<uint32_t>(0)) );
    return true;
}


