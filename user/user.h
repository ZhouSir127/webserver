#ifndef USER_H
#define USER_H
#include <unordered_map>
#include <mutex>
#include "../connection_pool/connection_pool.h"
#include <mysql/mysql.h>
#include "../log/log.h"
#include "../args.h"

class User{

private:
    std::unordered_map<std::string, std::string> userInfo;
    std::mutex lock;
    ConnectionPool connPool;
public:
    User(const SqlInfo& sqlInfo)
    :connPool(sqlInfo){
        MYSQL *mysql = nullptr;
        connectionRAII mysqlcon(mysql,&connPool);
    
        if (mysql_query(mysql, "SELECT username,passwd FROM user"))
            LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    
        MYSQL_RES *result = mysql_store_result(mysql);

        if(!result)
            throw std::exception();
        
        while (MYSQL_ROW row = mysql_fetch_row(result))
            userInfo[row[0] ] = row[1];
    
        mysql_free_result(result); 
    }
    
    bool add(const std::string&name,const std::string&password){
        MYSQL *mysql=nullptr;
        connectionRAII mysqlcon(mysql, &connPool);
        
        std::string sqlInsert = "INSERT INTO user(username, passwd) VALUES('" 
                        + name 
                        + "', '" 
                        + password 
                        + "')";

        if(mysql_query(mysql, sqlInsert.data() ) <0  ) 
            return false;
        std::unique_lock<std::mutex>Lock(lock);
        userInfo[name]=password;
        return true;
    }
    
    bool exists(const std::string&name){
        std::unique_lock <std::mutex>Lock(lock);
        return userInfo.find(name) != userInfo.end();
    }
    
    bool check(const std::string&name,const std::string&password){
        std::unique_lock <std::mutex>Lock(lock);
        return userInfo.find(name) != userInfo.end() && userInfo[name] == password;
    }
};


#endif