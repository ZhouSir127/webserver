#ifndef _REDIS_POOL_
#define _REDIS_POOL_

#include <sw/redis++/redis++.h>
#include <string>
#include <memory>
#include "../args.h"
#include "../log/log.h"

// 保持与 SqlInfo 一致的风格
class RedisPool {

public:
    // 获取内部封装的 Redis 操作实例
    std::shared_ptr<sw::redis::Redis> getRedis() {return redis;}
    
    RedisPool(const RedisInfo& redisInfo)
    {
        try {    
            sw::redis::ConnectionOptions connOptions; 
            connOptions.host = redisInfo.IP; 
            connOptions.port = redisInfo.port;
            connOptions.password = redisInfo.password;           
            
            sw::redis::ConnectionPoolOptions poolOptions;
            poolOptions.size = redisInfo.num; // 连接池容量

            redis = std::make_shared<sw::redis::Redis>(connOptions, poolOptions);
            
            redis->ping();
            
            LOG_INFO("Redis-plus-plus Connection Pool initialized successfully with %d connections", redisInfo.num);
            
        } catch (const sw::redis::Error &err) {
            LOG_ERROR("Redis Initialization Error: %s", err.what());
            exit(1);
        }
    }

private:
    // redis-plus-plus 的核心类，它内部自带了线程安全的连接池
    std::shared_ptr<sw::redis::Redis> redis;
};

#endif