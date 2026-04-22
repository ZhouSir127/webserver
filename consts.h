#ifndef CONSTS_H
#define CONSTS_H
#include <cstddef>
namespace consts{
    const int MAX_FD = 65536;       
    const size_t FILENAME_LEN = 200;
    const size_t READ_BUFFER_SIZE = 8192;
    const size_t WRITE_BUFFER_SIZE = 1024;
    const size_t MAX_EVENT_NUMBER = 10000; 
}

#endif