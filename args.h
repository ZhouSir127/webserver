#ifndef ARGS_H
#define ARGS_H

#include <string>
#include <cstdint>

struct SqlInfo{
    const std::string IP;
    const int port;
    const std::string account;
    const std::string password;
    const std::string name;
    const int num;
};

struct RedisInfo {
    const std::string IP;
    const int port;
    const std::string password; 
    const int num;              // 连接池大小
};

struct HttpInfo{
    const bool isConnectEt;
    const std::string root;
};

struct TimerInfo{
    const int lifeSpan;
    const unsigned int timeSlot;
};

struct ThreadPoolInfo{
    const size_t threadNumer;
    const size_t maxRequest;
};

struct LogInfo{
    const std::string file;
    const bool close;
    const size_t maxRequest;
};

struct ListenInfo{
    const uint16_t port;
    const bool isListenEt;
    const size_t backlog;
};

#endif