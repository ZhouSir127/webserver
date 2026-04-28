#include "connection_pool.h"
#include <cppconn/driver.h>
#include <string>
#include <stdlib.h>
#include <pthread.h>
#include "../log/log.h"

ConnectionPool::ConnectionPool(const SqlInfo& sqlInfo)
{
	try{
		sql::Driver* driver = get_driver_instance();
		std::string url = "tcp://" + sqlInfo.IP + ":" + std::to_string(sqlInfo.port);

		for (int i = 0; i < sqlInfo.num; i++){
			connQueue.emplace(driver->connect(url, sqlInfo.account, sqlInfo.password));
			connQueue.back()->setSchema(sqlInfo.name);
		}
	}catch (sql::SQLException &e){
		LOG_ERROR("MySQL Error: %s", e.what());
		exit(1);
	}
	LOG_INFO("MySQL Connection Pool initialized successfully with %d connections", sqlInfo.num);
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
std::unique_ptr<sql::Connection> ConnectionPool::getConnection()
{
	std::unique_lock<std::mutex>Lock(lock);
	cv.wait(Lock, [this]() -> bool { return !connQueue.empty(); } );

	std::unique_ptr<sql::Connection> con ( std::move(connQueue.front()) );
	connQueue.pop();

	return con;
}

//释放当前使用的连接
bool ConnectionPool::releaseConnection( std::unique_ptr<sql::Connection> con)
{
	if (!con)
		return false;

	if (!con->isValid()) { 
        LOG_ERROR("MySQL connection is dead, discarding it.", nullptr);
        return false; 
    }

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