#include "connection_pool.h"
#include <cppconn/driver.h>
#include <string>
#include <stdlib.h>
#include <pthread.h>
#include "../log/log.h"

ConnectionPool::ConnectionPool(const SqlInfo& sqlInfo)
:IP(sqlInfo.IP),
port(sqlInfo.port),
account(sqlInfo.account),
password(sqlInfo.password),
name(sqlInfo.name),
url ("tcp://" + IP + ":" + std::to_string(port) ),
driver(get_driver_instance() )
{
	try{
		for (int i = 0; i < sqlInfo.num; i++){
			connQueue.emplace(driver->connect(url,account, password));
			connQueue.back()->setSchema(sqlInfo.name);
		}
	}catch (sql::SQLException &e){
		LOG_ERROR("MySQL Error: ", e.what());
	}
	LOG_INFO("MySQL Connection Pool initialized successfully with ", sqlInfo.num, " connections");
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
std::unique_ptr<sql::Connection> ConnectionPool::getConnection()
{
	{
	std::unique_lock<std::mutex>Lock(lock);
	cv.wait(Lock, [this]() -> bool { return !connQueue.empty(); } );

	std::unique_ptr<sql::Connection> con ( std::move(connQueue.front()) );
	connQueue.pop();

	if (con && con->isValid())
        return con;
	}
	try {
		std::unique_ptr<sql::Connection> con = std::unique_ptr<sql::Connection>(driver->connect(url, account, password));
		con->setSchema(name);
		LOG_INFO("MySQL connection recreated successfully.");
		return con;
	} catch (sql::SQLException &e){
		LOG_ERROR("Failed to recreate MySQL connection: ", e.what());
		return nullptr;
	}
}

//释放当前使用的连接
bool ConnectionPool::releaseConnection( std::unique_ptr<sql::Connection> con)
{
	if (!con)
		return false;

	std::unique_lock<std::mutex>Lock(lock);
	connQueue.push(std::move(con) );

	cv.notify_one();
	return true;
}

connectionRAII::connectionRAII(ConnectionPool *connPoolPtr):conRAII(connPoolPtr->getConnection()), poolRAII(connPoolPtr)
{}

connectionRAII::~connectionRAII(){
	poolRAII->releaseConnection(std::move(conRAII));
}