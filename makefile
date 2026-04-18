CXX ?= g++

CXXFLAGS += -std=c++26 -Wall
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2
endif

server: main.cpp ./timer/lst_timer.cpp ./http/http_conn.cpp ./CGImysql/sql_connection_pool.cpp webserver.cpp config.cpp
	$(CXX) -o server $^ $(CXXFLAGS) -pthread -lmysqlclient -lmavsdk -lmavsdk_action -lmavsdk_telemetry

clean:
	rm -f server