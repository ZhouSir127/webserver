#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <cstdint>

namespace sysListen{
    const uint16_t PORT = 8080; 
    const bool LISTENET = false;
}

namespace timer{
    const int LIFE_SPAN = 3600;
    const unsigned int TIMESLOT = 5;
}

namespace http{
    const std::string ROOT = "/root/webserver/root";
    const bool CONNECTET = false;
}

namespace mysql{
    const std::string IP = "127.0.0.1";
    const int PORT = 3306;
    const std::string ACCOUNT = "root" ;
    const std::string PASSWORD = "4399";
    const std::string NAME = "webserver_db";
    const int NUM = 8;
}

namespace threadPool{
    const size_t NUM = 8;
    const size_t MAX_REQUEST = 10000;
}

namespace sysLog{
    const std::string FILE = "/var/log/webserver";
    const bool CLOSE = false;
}

namespace redis{
    const std::string IP = "127.0.0.1";
    const int PORT = 6379;
    const std::string PASSWORD = "4399";
    const int NUM = 8;
}

#endif