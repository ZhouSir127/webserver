CXX ?= g++
CXXFLAGS += -std=c++26 -Wall -I/usr/include/mavsdk
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2
endif

SRCS = main.cpp \
       webserver.cpp \
       http_conn/http_conn.cpp \
       thread_pool/thread_pool.cpp \
       timer_manager/timer_manager.cpp \
       epoll_manager/epoll_manager.cpp \
       log/log.cpp \
       CGImysql/sql_connection_pool.cpp

# 🔥 仅保留 -lmavsdk，删除所有插件库
server: $(SRCS)
	$(CXX) -o server $^ $(CXXFLAGS) -pthread -lmysqlclient -lmavsdk

clean:
	rm -f server