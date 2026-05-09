#ifndef CONSTS_H
#define CONSTS_H
#include <cstddef>
#include <string>
namespace consts{
    const int MAX_FD = 65536;       
    const size_t FILENAME_LEN = 200;
    const size_t READ_BUFFER_SIZE = 16384 ;
    const size_t WRITE_BUFFER_SIZE = 1024;
    const size_t MAX_EVENT_NUMBER = 10000; 
    const std::string_view SCHEME_HTTP = "http://";
    const std::string_view SCHEME_HTTPS = "https://";
    constexpr std::string_view HEADER_CONNECTION     = "Connection:";
    constexpr std::string_view HEADER_CONTENT_LENGTH = "Content-Length:";
    constexpr std::string_view HEADER_COOKIE         = "Cookie:";
    constexpr std::string_view VALUE_CLOSE           = "close";
}

#endif