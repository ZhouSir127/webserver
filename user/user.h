#ifndef USER_H
#define USER_H

#include "../redis_pool/redis_pool.h"
#include "../connection_pool/connection_pool.h"

class User{

private:
    ConnectionPool connPool;
    RedisPool redisPool;
public:
    User(const SqlInfo& sqlInfo,const RedisInfo& redisInfo)
    :connPool(sqlInfo),redisPool(redisInfo){}
    
    bool add(const std::string& name,const std::string& password );
    std::string generateUUID();
    std::string login(const std::string&name,const std::string&password);
    bool verify(const std::string& token);
};


#endif