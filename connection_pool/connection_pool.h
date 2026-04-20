#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <deque>
#include <mysql/mysql.h>
#include <error.h>
#include <string>
#include <semaphore.h>
#include <mutex>

class ConnectionPool
{
public:
	MYSQL *getConnection();				 //获取数据库连接
	bool releaseConnection(MYSQL *conn); //释放连接
	//int getFreeConn();					 //获取连接
	
	ConnectionPool(const std::string& url, int port, const std::string&account,const std::string& password,const std:: string& name, int num);
	~ConnectionPool();
private:
	std::mutex lock;
	std::deque <MYSQL *> connQueue; //连接池
	sem_t reserve;
};

class connectionRAII{

public:
	connectionRAII(MYSQL * &con, ConnectionPool *connPool);
	~connectionRAII();
	
private:
	MYSQL *conRAII;
	ConnectionPool *poolRAII;
};

#endif
