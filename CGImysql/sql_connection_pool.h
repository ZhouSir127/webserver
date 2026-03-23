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
	
	connection_pool(char* url, int Port, char* User, char* PassWord, char* DBName, int MaxConn);
	~connection_pool();
private:
	int m_MaxConn;
	locker lock;
	std::deque <MYSQL *> connList; //连接池
	sem reserve;
};

class connectionRAII{

public:
	connectionRAII(MYSQL **con, connection_pool *connPool);
	~connectionRAII();
	
private:
	MYSQL *conRAII;
	connection_pool *poolRAII;
};

#endif
