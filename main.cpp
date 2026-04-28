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

    struct ListenInfo listenInfo{
        sysListen::PORT,
        sysListen::LISTENET
    };

    struct ThreadPoolInfo threadPoolInfo{
        threadPool::NUM,
        threadPool::MAX_REQUEST
    };

    struct LogInfo logInfo{
        sysLog::FILE,
        sysLog::CLOSE
    };

    struct RedisInfo redisInfo{
        redis::IP,
        redis::PORT,
        redis::PASSWORD,
        redis::NUM
    };

    WebServer server(
            listenInfo,
            timerInfo,
            httpInfo,sqlInfo,redisInfo,
            threadPoolInfo,
            logInfo,
        );
    //运行
    server.eventLoop();

    return 0;
}