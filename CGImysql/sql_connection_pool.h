#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <deque>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

class connection_pool
{
public:
	MYSQL *GetConnection();				 //获取数据库连接
	bool ReleaseConnection(MYSQL *conn); //释放连接
	int GetFreeConn();					 //获取连接
	
	connection_pool(const std::string& url, int Port, const std::string&User,const std::string& PassWord,const std:: string& DBName, int MaxConn);
	~connection_pool();
private:
	//int m_MaxConn;
	std::mutex lock;
	std::deque <MYSQL *> connList; //连接池
	sem_t reserve;
};

class connectionRAII{

public:
	connectionRAII(MYSQL * &con, connection_pool *connPool);
	~connectionRAII();
	
private:
	MYSQL *conRAII;
	connection_pool *poolRAII;
};

#endif
