#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <deque>
#include <cppconn/connection.h> 
#include <error.h>
#include <semaphore.h>
#include <mutex>
#include <memory>
#include "../args.h"

class ConnectionPool
{
public:
	std::unique_ptr<sql::Connection> getConnection();				 //获取数据库连接
	bool releaseConnection(std::unique_ptr<sql::Connection> conn); //释放连接
	
	ConnectionPool(const SqlInfo& sqlInfo);
	~ConnectionPool();
private:
	std::mutex lock;
	std::deque <std::unique_ptr<sql::Connection> > connQueue; //连接池
	sem_t reserve;
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
