#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <queue>
#include <cppconn/connection.h> 
#include <error.h>
#include <condition_variable>
#include <mutex>
#include <memory>
#include "../args.h"

class ConnectionPool
{
public:
	std::unique_ptr<sql::Connection> getConnection();				 //获取数据库连接
	bool releaseConnection(std::unique_ptr<sql::Connection> conn); //释放连接
	
	ConnectionPool(const SqlInfo& sqlInfo);
private:
	std::mutex lock;
	std::queue <std::unique_ptr<sql::Connection> > connQueue; //连接池
	std::condition_variable cv; //连接池条件变量

	const std::string IP;
	const uint16_t port;
	const std::string account;
	const std::string password;
	const std::string name;
	const std::string url;
	sql::Driver* driver;
};

class connectionRAII{

public:
	connectionRAII(ConnectionPool *connPool);
	~connectionRAII();

	sql::Connection* getConnection() const { return conRAII.get(); }
	
private:
	std::unique_ptr<sql::Connection> conRAII;
	ConnectionPool *poolRAII;
};

#endif
