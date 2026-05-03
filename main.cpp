#include "config.h"
#include "webserver.h"

int main()
{
    struct TimerInfo timerInfo{
        timer::LIFE_SPAN,
        timer::TIMESLOT
    };

    struct HttpInfo httpInfo{
        http::CONNECTET,
        http::ROOT
    };

    struct SqlInfo sqlInfo{
        mysql::IP,
        mysql::PORT,
        mysql::ACCOUNT,
        mysql::PASSWORD,
        mysql::NAME,
        mysql::NUM
    };

    struct RedisInfo redisInfo{
        redis::IP,
        redis::PORT,
        redis::PASSWORD,
        redis::NUM
    };

    struct ThreadPoolInfo threadPoolInfo{
        threadPool::NUM,
        threadPool::MAX_REQUEST
    };

    struct ListenInfo listenInfo{
        sysListen::PORT,
        sysListen::LISTENET,
        sysListen::BACKLOG
    };

    struct LogInfo logInfo{
        sysLog::FILE,
        sysLog::CLOSE,
        sysLog::MAX_REQUEST
    };
    myLog::init(logInfo);
    WebServer server(
                timerInfo,
                httpInfo,sqlInfo,redisInfo,
                threadPoolInfo,
                listenInfo
        );
    //运行
    server.eventLoop();

    return 0;
}