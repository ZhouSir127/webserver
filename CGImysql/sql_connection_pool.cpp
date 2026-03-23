#include "sql_connection_pool.h"
#include "../log/log.h"
#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>

connection_pool::connection_pool(char* url, int Port,char* User, char* PassWord, char* DBName, int MaxConn)
:m_MaxConn(MaxConn),reserve(sem(m_MaxConn) )
{
	for (int i = 0; i < m_MaxConn; i++)
	{
		MYSQL *con = mysql_init(nullptr);

		if (con == NULL)
		{
		//	LOG_ERROR("MySQL Error");
			exit(1);
		}
		con = mysql_real_connect(con, url, User, PassWord, DBName, Port, nullptr , 0);

		if (con == NULL)
		{
		//	LOG_ERROR("MySQL Error");
			exit(1);
		}
		
		connList.push_back(con);
	}

}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
	MYSQL *con = nullptr;

	if (!connList.size() )
		return nullptr;

	reserve.wait();
	
	lock.lock();

	con = connList.front();
	connList.pop_front();

	lock.unlock();
	return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	if (!con)
		return false;

	lock.lock();

	connList.push_back(con);

	lock.unlock();

	reserve.post();
	return true;
}

//当前空闲的连接数
int connection_pool::GetFreeConn()
{
	return connList.size();
}

connection_pool::~connection_pool()
{
	lock.lock();
	
	if (connList.size() > 0)
		for (deque <MYSQL *>::iterator it = connList.begin(); it != connList.end(); ++it)
			mysql_close(*it);
	
	lock.unlock();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPoolptr){
	*SQL = connPoolptr->GetConnection();
	
	conRAII = *SQL;
	poolRAII = connPoolptr;
}

connectionRAII::~connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}