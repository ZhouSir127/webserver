# ---------------------------------------------------------
# 编译器设置
# ---------------------------------------------------------
CXX          ?= g++
TARGET       := server

# ---------------------------------------------------------
# 严苛的编译选项 (去掉了 -Wshadow，增加了逻辑与标准检查)
# ---------------------------------------------------------
# -Wduplicated-cond: 检查 if-else 中重复的条件
# -Wduplicated-branches: 检查 if-else 不同分支代码是否完全一致
# -Wlogical-op: 检查可疑的逻辑运算 (如 x < 0 && x > 10)
# -Wnull-dereference: 尽力检测可能的空指针解引用
# -Wdouble-promotion: 检查 float 是否被隐式提升为 double (性能损耗点)
# -Wextra-semi: 检查多余的分号
# -Wuseless-cast: 检查无意义的类型转换
STRICT_FLAGS := -Wall -Wextra -Werror -Wpedantic \
                -Wduplicated-cond -Wduplicated-branches -Wlogical-op \
                -Wnull-dereference -Wdouble-promotion -Wextra-semi \
                -Wuseless-cast -Wformat=2 -Wconversion -Wunused \
                -Wold-style-cast -fstack-protector-all

CXXFLAGS     := -std=c++23 $(STRICT_FLAGS) -I/usr/include/mavsdk

# ---------------------------------------------------------
# 运行时严苛检查 (Sanitizers) - 这是提升稳定性的杀手锏
# ---------------------------------------------------------
# Address Sanitizer (ASan): 检测内存泄漏、越界访问、Use-after-free
# Undefined Behavior Sanitizer (UBSan): 检测整数溢出、除零等未定义行为
# 注：开启此项会轻微降低运行速度，但能捕捉 90% 的内存 Bug
SAN_FLAGS    := -fsanitize=address -fsanitize=undefined

# ---------------------------------------------------------
# 调试与优化选项
# ---------------------------------------------------------
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    # 调试模式：开启 Sanitizers 和 STL 边界检查
    CXXFLAGS += -g -O0 -D_GLIBCXX_ASSERTIONS $(SAN_FLAGS)
    LDFLAGS  += $(SAN_FLAGS)
else
    # 发布模式：极限优化，关闭 Sanitizers 以换取极致性能
    CXXFLAGS += -O3 -DNDEBUG
endif

# ---------------------------------------------------------
# 路径与库设置
# ---------------------------------------------------------
LDFLAGS      += -L/usr/lib/x86_64-linux-gnu -L/usr/local/lib
LIBS         := -pthread -lmysqlclient -lmavsdk

# ---------------------------------------------------------
# 源文件与依赖管理
# ---------------------------------------------------------
SRCS         := main.cpp \
                webserver.cpp \
                http_conn/http_conn.cpp \
                thread_pool/thread_pool.cpp \
                timer_manager/timer_manager.cpp \
                epoll_manager/epoll_manager.cpp \
                log/log.cpp \
                CGImysql/sql_connection_pool.cpp

OBJS         := $(SRCS:.cpp=.o)
DEPS         := $(SRCS:.cpp=.d)

# ---------------------------------------------------------
# 构建规则
# ---------------------------------------------------------

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking with Sanitizers: $(TARGET)..."
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LIBS)

%.o: %.cpp
	@echo "Compiling with strict checks: $<..."
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

# ---------------------------------------------------------
# 功能指令
# ---------------------------------------------------------

clean:
	rm -f $(TARGET) $(OBJS) $(DEPS)

rebuild: clean all

.PHONY: all clean rebuild run

run: $(TARGET)
	@echo "\n[1/2] Cleaning up port 8080..."
	-@fuser -k 8080/tcp 2>/dev/null || true
	@echo "[2/2] Running $(TARGET) (ASan/UBSan Active)...\n"
	./$(TARGET)