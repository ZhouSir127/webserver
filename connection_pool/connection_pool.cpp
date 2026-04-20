#include "connection_pool.h"
#include <mysql/mysql.h>
#include <string>
#include <stdlib.h>
#include <pthread.h>
#include "../log/log.h"

ConnectionPool::ConnectionPool(const std::string& url, int port, const std::string& account, const std::string& password, const std::string& name, int num)
{
	sem_init(&reserve, 0, num);

	for (int i = 0; i < num; i++)
	{
		MYSQL *con = mysql_init(nullptr);

		if (!con){
			LOG_ERROR("MySQL Error: mysql_init failed");
			exit(1);
		}

		con = mysql_real_connect(con, url.c_str(), account.c_str(), password.c_str(), name.c_str(), port, nullptr , 0);

		if (!con){
			LOG_ERROR("MySQL Error: mysql_real_connect failed for User: %s", account.c_str());
			exit(1);
		}
		connQueue.push_back(con);
	}
	LOG_INFO("MySQL Connection Pool initialized successfully with %d connections", num);
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *ConnectionPool::getConnection()
{
	sem_wait(&reserve);
	
	std::unique_lock<std::mutex>Lock(lock);

	MYSQL *con = connQueue.front();
	connQueue.pop_front();

	return con;
}

//释放当前使用的连接
bool ConnectionPool::releaseConnection(MYSQL *con)
{
	if (!con)
		return false;

	std::unique_lock<std::mutex>Lock(lock);
	connQueue.push_back(con);

	sem_post(&reserve);
	return true;
}


ConnectionPool::~ConnectionPool()
{
	sem_destroy(&reserve);

	std::unique_lock<std::mutex>Lock(lock);

	for (std::deque <MYSQL *>::iterator it = connQueue.begin(); it != connQueue.end(); ++it)
		mysql_close(*it);
}

connectionRAII::connectionRAII(MYSQL * &con, ConnectionPool *connPoolPtr){
	con = connPoolPtr->getConnection();
	
	conRAII = con;
	poolRAII = connPoolPtr;
}

connectionRAII::~connectionRAII(){
	poolRAII->releaseConnection(conRAII);
}