#include "config.h"
#include "webserver.h"

int main()
{

    struct SqlInfo sqlInfo{
        mysql::IP,
        mysql::PORT,
        mysql::ACCOUNT,
        mysql::PASSWORD,
        mysql::NAME,
        mysql::NUM
    };
    
    struct HttpInfo httpInfo{
        http::CONNECTET,
        http::ROOT
    };

    struct TimerInfo timerInfo{
        timer::LIFE_SPAN,
        timer::TIMESLOT
    };

    struct EpollInfo epollInfo{
        webserver::LISTENET,
        http::CONNECTET
    };

    struct ServerInfo ServerInfo{
        webserver::PORT,
        webserver::LISTENET,
        
    };

    struct ThreadPoolInfo threadPoolInfo{
        threadPool::NUM,
        threadPool::MAX_REQUEST
    };

    struct LogInfo logInfo{
        sysLog::FILE,
        sysLog::CLOSE
    };

    WebServer server(
            ServerInfo,
            epollInfo,
            timerInfo,
            httpInfo,sqlInfo,
            threadPoolInfo,
            logInfo
        );
    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}