#ifndef ARGS_H
#define ARGS_H

#include <string>
#include <cstdint>

struct SqlInfo{
    const std::string&IP;
    int port;
    const std::string&account;
    const std::string&password;
    const std::string&name;
    int num;
};

struct HttpInfo{
    bool isConnectEt;
    const std::string&root;
};

struct TimerInfo{
    int lifeSpan;
    unsigned int timeSlot;
};

struct EpollInfo{
    bool isListenEt;
    bool isConnectEt;
};

struct ServerInfo{
    uint16_t port; 
    bool  isListenEt;
};

struct ThreadPoolInfo{
    size_t threadNumer;
    size_t maxRequest;
};

struct LogInfo{
    const std::string&file;
    bool close;
};

#endif