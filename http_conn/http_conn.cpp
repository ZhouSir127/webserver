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
    while (checkedIdx < endIdx ){
        int next = (checkedIdx + 1)%consts::READ_BUFFER_SIZE; 
        if ( readBuffer[checkedIdx] == '\r'){
            if ( next == endIdx ){
                status = HttpCode::NO_REQUEST;
                return;
            }else if ( readBuffer[next] == '\n'){
                if(startIdx <= checkedIdx)
                    line = std::string(readBuffer.begin() + startIdx , readBuffer.begin() + checkedIdx);
                else
                    line = readBuffer.substr(startIdx)+std::string(readBuffer.begin(),readBuffer.begin() + checkedIdx ) ;
                
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

    if ( strncasecmp(token.c_str(),"http://",7) == 0){
        pos = token.find_first_of('/',7);
        if (pos == std::string::npos ){
            status = HttpCode::BAD_REQUEST;
            return;
        }
    }else if (strncasecmp(token.c_str(),"https://",8) == 0){
        pos = token.find_first_of('/',8);
        if ( pos == std::string::npos ){
            status = HttpCode::BAD_REQUEST;
            return;
        }
    }else if(token[pos] != '/'){
        status = HttpCode::BAD_REQUEST;
        return;
    }
    
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
    {
        checkState = CheckState::CHECK_STATE_CONTENT;       
    }else if (strncasecmp(line.c_str(), "Connection:",11) == 0){
        size_t pos = line.find_first_not_of ( " \t" , 11 );
        if(pos == std::string::npos){
            status = HttpCode::BAD_REQUEST;
            return;
        }
        if ( strncasecmp( line.c_str()+pos,"close",5) == 0)
            isLinger = false;
    }else if (strncasecmp(line.c_str(), "Content-Length:",15) == 0){
        size_t pos = line.find_first_not_of ( " \t" , 15 );
        if(pos == std::string::npos){
            status = HttpCode::BAD_REQUEST;
            return;
        }
        contentLength = std::stoul(std::string(line.substr(pos)) );
    }else if(strncasecmp(line.c_str(),  "Cookie:",7) == 0){
        size_t pos = line.find_first_not_of ( " \t" , 7 );
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
            else if ( (endIdx+ consts::READ_BUFFER_SIZE - startIdx)%consts::READ_BUFFER_SIZE >= contentLength )
            {
                if(startIdx + contentLength <= consts::READ_BUFFER_SIZE ){
                    requestBody = readBuffer.substr(startIdx ,contentLength);
                    startIdx += contentLength;
                }else{
                    int newStartIdx= contentLength - (consts::READ_BUFFER_SIZE-startIdx);
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
            if (status != HttpCode::GET_REQUEST )
                return;
            
            if(checkState == CheckState::CHECK_STATE_REQUESTLINE ){
                parseRequestLine();
                if ( status != HttpCode::GET_REQUEST)
                    return;
            }else if(checkState == CheckState::CHECK_STATE_HEADER){
                parseHeaders();
                if ( status != HttpCode::GET_REQUEST)
                    return;
            }
        }
    status = HttpCode::GET_REQUEST;
}

void Message::prepare(Router&router){
    
    if(status == HttpCode::NO_REQUEST)//GET,NO,BAD
        return;

    if(status == HttpCode::GET_REQUEST ){
        router.route(this);
        prepareFile();
    }
    if( status == HttpCode::BAD_REQUEST || status == HttpCode::FORBIDDEN_REQUEST || status == HttpCode::NO_RESOURCE || status == HttpCode::INTERNAL_ERROR){
        isLinger = false;
        LOG_WARN("Bad HTTP request syntax" );
    }
    if ( !prepareHeaders(status) )
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
        
        ioVectorCount = 2;
        ioVectors[1].iov_len = fileSize = std::filesystem::file_size(realFilePath);
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




