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



HttpCode Message::parseLine()
{
    while (checkedIdx < readBuffer.size() ){
        if ( readBuffer[checkedIdx] == '\r'){
            if ( checkedIdx + 1 == readBuffer.size() )
                return HttpCode::NO_REQUEST;
            else if ( readBuffer[checkedIdx+1] == '\n'){
                line = std::string(readBuffer.begin() + startIdx , readBuffer.begin() + checkedIdx);
                
                checkedIdx += 2;
                startIdx = checkedIdx;

                return HttpCode::GET_REQUEST;
            }else
                return HttpCode::BAD_REQUEST;
        }
        ++checkedIdx;
    }
    return HttpCode::NO_REQUEST;
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
// HttpCode HttpConn::read()
// {
//     //LT读取数据
//     if (!isConnectEt){
//         if( readBuffer.size() == readIdx ){
//             if(readBuffer.size() == consts::READ_BUFFER_SIZE){
//                 LOG_WARN("Read buffer overflow (LT). Malicious client? fd: ", fd);
//                 return HttpCode::GET_REQUEST;
//             }
//             readBuffer.resize( std::min( (readBuffer.size()<<1),consts::READ_BUFFER_SIZE) );
//         }
//         int bytesRead = recv(fd, &readBuffer[readIdx] ,readBuffer.size()-readIdx,0);
    
//         if (bytesRead < 0 ){
//             if (errno == EAGAIN || errno == EINTR)
//                 return HttpCode::GET_REQUEST;
//             return HttpCode::BAD_REQUEST;
//         }else if (bytesRead == 0)
//             return HttpCode::CLOSED_CONNECTION;
        
//         readIdx += bytesRead;
//     }else
//         while (true){    
//             if(readBuffer.size()==readIdx){
//                 if(readBuffer.size() == consts::READ_BUFFER_SIZE){
//                     LOG_WARN("Read buffer overflow (ET). Malicious client? fd: ", fd);
//                     break;
//                 }
//                 readBuffer.resize(std::min( (readBuffer.size()<<1),consts::READ_BUFFER_SIZE) );
//             }
//             int bytesRead = recv(fd, &readBuffer[readIdx], readBuffer.size()-readIdx , 0);
//             if (bytesRead < 0 ){
//                 if (errno == EAGAIN || errno == EINTR)
//                     break;
//                 return HttpCode::BAD_REQUEST;
//             }else if (bytesRead == 0)
//                 return HttpCode::CLOSED_CONNECTION;
            
//             readIdx += bytesRead;
//         }
//     return HttpCode:: GET_REQUEST;
// }

//解析http请求行，获得请求方法，目标url及http版本号
HttpCode Message::parseRequestLine()
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

HttpCode Message::parseHeaders()
{
    if ( line.empty() )
    {
        checkState = CheckState::CHECK_STATE_CONTENT;       
    }else if (strncasecmp(line.c_str(), "Connection:",11) == 0){
        size_t pos = line.find_first_not_of ( " \t" , 11 );
        if(pos == std::string::npos)
            return HttpCode::BAD_REQUEST;
        if ( strncasecmp( line.c_str()+pos,"close",5) == 0)
            isLinger = false;
    }else if (strncasecmp(line.c_str(), "Content-Length:",15) == 0){
        size_t pos = line.find_first_not_of ( " \t" , 15 );
        if(pos == std::string::npos)
            return HttpCode::BAD_REQUEST;
        contentLength = std::stoul(std::string(line.begin()+pos , line.end() ) );
    }else if(strncasecmp(line.c_str(),  "Cookie:",7) == 0){
        size_t pos = line.find_first_not_of ( " \t" , 7 );
        if(pos == std::string::npos)
            return HttpCode::BAD_REQUEST;
        cookie = std::string(line.begin()+pos, line.end() );
    }
    
    return HttpCode::GET_REQUEST;
}

HttpCode Message::processRead()
{
    while(true)
        if(checkState == CheckState::CHECK_STATE_CONTENT){
            if (contentLength == 0) 
                break;
            else if (readBuffer.size() - checkedIdx >= contentLength )
            {
                requestBody = readBuffer.substr(startIdx ,contentLength);
                checkedIdx = startIdx + contentLength ;
                startIdx = checkedIdx;    
                break;
            }else
                return HttpCode::NO_REQUEST;
        }else{
            HttpCode ret = parseLine();
            if (ret != HttpCode::GET_REQUEST )
                return ret;
            
            if(checkState == CheckState::CHECK_STATE_REQUESTLINE ){
                ret = parseRequestLine();
                if ( ret != HttpCode::GET_REQUEST)
                    return ret;
            }else{
                ret = parseHeaders();
                if ( ret != HttpCode::GET_REQUEST)
                    return ret;
            }
        }
    return HttpCode::GET_REQUEST;
}

HttpCode Message::process(Router&router){
    
    HttpCode ret = processRead();//GET,NO,BAD
    
    if(ret == HttpCode::NO_REQUEST)
        return ret;

    if(ret == HttpCode::GET_REQUEST ){
        router.route(this);
        ret = prepareFile();
    }

    if( ret == HttpCode::BAD_REQUEST || ret == HttpCode::FORBIDDEN_REQUEST || ret == HttpCode::NO_RESOURCE || ret == HttpCode::INTERNAL_ERROR){
        isLinger = false;
        LOG_WARN("Bad HTTP request syntax" );
    }
    if ( !processWrite(ret) )
        return HttpCode::CLOSED_CONNECTION;

    return ret;
}

HttpCode Message::prepareFile()
{   
    if(realFilePath.empty() == false ){
        if (std::filesystem::exists(realFilePath) == false  ){
            LOG_WARN("404 Not Found requested. path: ", realFilePath);
            return HttpCode::NO_RESOURCE;
        }
        if (access(realFilePath.c_str(), R_OK) != 0  ){
            LOG_ERROR("File permission denied (403). WebServer lacks R_OK for path: ", realFilePath);
            return HttpCode::FORBIDDEN_REQUEST;
        }
        if (std::filesystem::is_regular_file(realFilePath) == false )
            return HttpCode::BAD_REQUEST;
    
        ioVectorCount = 2;
        ioVectors[1].iov_len = fileSize = std::filesystem::file_size(realFilePath);
        int fd = open(realFilePath.data(), O_RDONLY);
        ioVectors[1].iov_base = fileAddress = static_cast<char*>(mmap(0, fileSize,PROT_READ, MAP_PRIVATE, fd, 0));
        close(fd);
        if (fileAddress == MAP_FAILED) {
            LOG_ERROR("mmap failed for file: ", realFilePath, " errno: ", errno);
            fileAddress = nullptr;
            return HttpCode::INTERNAL_ERROR;
        }
        ioVectors[1].iov_len = fileSize = std::filesystem::file_size(realFilePath);
        
        return HttpCode::FILE_REQUEST;
    }
    return HttpCode::GET_REQUEST;
}

bool Message::processWrite(HttpCode ret)
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




