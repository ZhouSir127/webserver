#include "user.h"


#include <cppconn/prepared_statement.h>
#include <memory>
#include <random>
#include "../log/log.h"
#include "../crypto_util/crypto_util.h"

bool User::add(const std::string& name,const std::string& password ) {
    try{
        connectionRAII mysqlcon(&connPool);
        sql::Connection* con = mysqlcon.getConnection();

        if (!con) {
            LOG_ERROR("Failed to get database connection from pool.");
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> queryPstmt(con->prepareStatement("SELECT username FROM user WHERE username = ?") );
        queryPstmt -> setString(1,name);
        std::unique_ptr<sql::ResultSet> res(queryPstmt -> executeQuery());
        if(res -> next() )
            return false;

        std::unique_ptr<sql::PreparedStatement> Insertpstmt(con->prepareStatement("INSERT INTO user(username, passwd) VALUES(?, ?)") );
        Insertpstmt -> setString(1,name);
        
        std::string salt = cryptoUtil::generateSalt();
        std::string hashedPassword = cryptoUtil::hashPassword(password, salt);

        Insertpstmt -> setString(2,salt + "$" +hashedPassword);
        return Insertpstmt -> executeUpdate() > 0;
    }catch(sql::SQLException &e){
        LOG_ERROR("SQL error: ", e.what());
        return false;
    }
}

std::string User::generateUUID() {

    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    thread_local std::uniform_int_distribution<uint16_t> dis(0, 255);

    uint8_t uuid[16];
    for (int i = 0; i < 16; ++i)
        uuid[i] = static_cast<uint8_t>(dis(gen));

    uuid[6] = (uuid[6] & 0x0F) | 0x40; // 版本号 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80; // 变体号


    char buf[37];
    snprintf(buf, sizeof(buf),
                "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                uuid[0], uuid[1], uuid[2], uuid[3],
                uuid[4], uuid[5],
                uuid[6], uuid[7],
                uuid[8], uuid[9],
                uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

    return std::string(buf);
}

std::string User::login(const std::string&name,const std::string&password){
    try{
        connectionRAII mysqlcon(&connPool);
        sql::Connection* con = mysqlcon.getConnection();

        if (!con) {
            LOG_ERROR("Failed to get database connection from pool.");
            return ""; // 或者在 login 里 return "";
        }        
        std::unique_ptr<sql::PreparedStatement> queryPstmt(con->prepareStatement("SELECT passwd FROM user WHERE username = ?") );
        queryPstmt -> setString(1,name);

        std::unique_ptr<sql::ResultSet> res(queryPstmt -> executeQuery());
        if(res -> next() ){
            std::string storedPassword = res -> getString("passwd");
            size_t delimiterPos = storedPassword.find('$');
            if (delimiterPos == std::string::npos) {
                LOG_ERROR("Stored password format error for user: ", name.c_str());
                return "";
            }
            std::string salt = storedPassword.substr(0, delimiterPos); //[0:delimiterPos-1]
            std::string hashedPassword = storedPassword.substr(delimiterPos + 1);//[delimiterPos+1,end]

            if( cryptoUtil::hashPassword(password, salt) == hashedPassword ){
                std::shared_ptr<sw::redis::Redis> redis = redisPool.getRedis();
                std::string token = generateUUID();
                redis->setex(token, 3600, name);
                return token;
            }
        }
        return "";
    }catch(sql::SQLException &e){
        LOG_ERROR("SQL error: ", e.what());
        return "";
    }
}
bool User::verify(const std::string& token){
    try{
        std::shared_ptr<sw::redis::Redis> redis = redisPool.getRedis();
        std::optional<std::string>name = redis->get(token);
        if(name){
            redis -> expire(token, 3600);
            return true;
        }else
            return false;
    }catch(const sw::redis::Error &e){
        LOG_ERROR("Redis error: ", e.what());
        return false;
    }
}