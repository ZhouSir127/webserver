#ifndef USER_H
#define USER_H

#include "../connection_pool/connection_pool.h"
#include <cppconn/prepared_statement.h>
#include "../log/log.h"
#include "../args.h"
#include "../redis_pool/redis_pool.h"
#include <memory>

class User{

private:
    ConnectionPool connPool;
    RedisPool redisPool;
public:
    User(const SqlInfo& sqlInfo,const RedisInfo& redisInfo)
    :connPool(sqlInfo),redisPool(redisInfo){
        srand(time(NULL)); 
    }
    
    bool add(const std::string& name,const std::string& password ) {
        try{
            connectionRAII mysqlcon(&connPool);
            sql::Connection* con = mysqlcon.getConnection();

            std::unique_ptr<sql::PreparedStatement> queryPstmt(con->prepareStatement("SELECT username FROM user WHERE username = ?") );
            queryPstmt -> setString(1,name);
            std::unique_ptr<sql::ResultSet> res(queryPstmt -> executeQuery());
            if(res -> next() )
                return false;

            std::unique_ptr<sql::PreparedStatement> Insertpstmt(con->prepareStatement("INSERT INTO user(username, passwd) VALUES(?, ?)") );
            Insertpstmt -> setString(1,name);
            Insertpstmt -> setString(2,password);
            return Insertpstmt -> executeUpdate() > 0;
        }catch(sql::SQLException &e){
            LOG_ERROR("SQL error: %s", e.what());
            return false;
        }
    }

    std::string login(const std::string&name,const std::string&password){
        try{
            connectionRAII mysqlcon(&connPool);
            sql::Connection* con = mysqlcon.getConnection();

            std::unique_ptr<sql::PreparedStatement> queryPstmt(con->prepareStatement("SELECT username FROM user WHERE username = ? AND passwd = ?") );
            queryPstmt -> setString(1,name);
            queryPstmt -> setString(2,password);
            std::unique_ptr<sql::ResultSet> res(queryPstmt -> executeQuery());
            if(res -> next() ){
                std::shared_ptr<sw::redis::Redis> redis = redisPool.getRedis();
                std::string token = std::to_string( rand() );
                redis->setex(token, 3600, name);
                
                return token;
            }else
                return "";
            
        }catch(sql::SQLException &e){
            LOG_ERROR("SQL error: %s", e.what());
            return "";
        }
    }


};


#endif