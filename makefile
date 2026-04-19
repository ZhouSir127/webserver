CXX ?= g++
# 降级为更通用的C++23（或C++17），避免版本兼容问题
CXXFLAGS += -std=c++23 -Wall -Wextra -I/usr/include/mavsdk
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g -O0
else
    CXXFLAGS += -O2 -DNDEBUG
endif

# 确保所有源文件路径正确
SRCS = main.cpp \
       webserver.cpp \
       http_conn/http_conn.cpp \
       thread_pool/thread_pool.cpp \
       timer_manager/timer_manager.cpp \
       epoll_manager/epoll_manager.cpp \
       log/log.cpp \
       CGImysql/sql_connection_pool.cpp

# 显式指定库路径（示例，根据实际路径调整）
LDFLAGS = -L/usr/lib/x86_64-linux-gnu -L/usr/local/lib

server: $(SRCS)
	$(CXX) -o server $^ $(CXXFLAGS) $(LDFLAGS) -pthread -lmysqlclient -lmavsdk

clean:
	rm -f server
	# 可选：清理编译过程中生成的临时文件
	rm -rf *.o http_conn/*.o thread_pool/*.o timer_manager/*.o epoll_manager/*.o log/*.o CGImysql/*.o