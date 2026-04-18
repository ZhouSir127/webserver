#include <string>

namespace webserver{
    const int PORT = 8080; 
    const bool LISTENET = false;
    const bool CONNECTET = false;
};

namespace timer{
    const int LIFE_SPAN = 3600; 
    const int TIMESLOT = 5;
};

namespace http{
    const std::string ROOT = "../root";
};

namespace mysql{
    const std::string IP = "127.0.0.1";
    const int PORT = 3306;
    const std::string USER = "root" ;
    const std::string PASSWORD = "4399";
    const std::string NAME = "MySQL";
    const int NUM = 8;
};

namespace threadPool{
    const int NUM = 8;
    const int MAX_REQUEST = 10000;
};

namespace log{
    const std::string FILE = "/var/log/webserver";
    const bool CLOSE = false;
}
