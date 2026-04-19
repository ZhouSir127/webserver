#include "sql_connection_pool.h"
#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "../log/log.h"

connection_pool::connection_pool(const std::string& url, int Port,const std::string& User, const std::string& PassWord, const std::string& DBName, int MaxConn)
{
	sem_init(&reserve, 0, MaxConn);

	for (int i = 0; i < MaxConn; i++)
	{
		MYSQL *con = mysql_init(nullptr);

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error: mysql_init failed");
			exit(1);
		}

		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, nullptr , 0);

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error: mysql_real_connect failed for User: %s", User.c_str());
			exit(1);
		}
		connList.push_back(con);
	}
	LOG_INFO("MySQL Connection Pool initialized successfully with %d connections", MaxConn);
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
	sem_wait(&reserve);
	
	std::unique_lock<std::mutex>Lock(lock);

	MYSQL *con = connList.front();
	connList.pop_front();

	return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	if (!con)
		return false;

	std::unique_lock<std::mutex>Lock(lock);
	connList.push_back(con);

	sem_post(&reserve);
	return true;
}

//当前空闲的连接数
int connection_pool::GetFreeConn()
{
	return connList.size();
}

connection_pool::~connection_pool()
{
	sem_destroy(&reserve);

	std::unique_lock<std::mutex>Lock(lock);

	for (std::deque <MYSQL *>::iterator it = connList.begin(); it != connList.end(); ++it)
		mysql_close(*it);
}

connectionRAII::connectionRAII(MYSQL * &con, connection_pool *connPoolptr){
	con = connPoolptr->GetConnection();
	
	conRAII = con;
	poolRAII = connPoolptr;
}

connectionRAII::~connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}