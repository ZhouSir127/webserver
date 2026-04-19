#include "config.h"
#include "webserver.h"

int main(int argc, char *argv[])
{
    WebServer server(webserver::PORT,
            timer::LIFE_SPAN,timer::TIMESLOT,
            webserver::LISTENET,webserver::CONNECTET,
            mysql::IP,mysql::PORT,mysql::USER,mysql::PASSWORD,mysql::NAME,mysql::NUM,http::ROOT,
            threadPool::NUM, threadPool::MAX_REQUEST,
            sysLog::FILE,sysLog::CLOSE        
        );
    
    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}