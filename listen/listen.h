#ifndef LISTEN_H 
#define LISTEN_H

class Listen
{
public:
    Listen(uint16_t port, bool isListenEt):port(port),isListenEt(isListenEt) {}
    ~Listen() = default;
    uint16_t getPort() const { return port; }       


};

#endif
