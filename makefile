# ============================================================================
# Top-Tier Strict C++ WebServer Makefile
# 宽松：简单数据类型隐式转换 | 严格：其余所有安全/语法校验
# 增量编译：仅重新编译修改过的文件，不编译全量
# ============================================================================

CXX      := g++
TARGET   := server

# ----------------------------------------------------------------------------
# 1. 核心标准与架构
# ----------------------------------------------------------------------------
CXXFLAGS := -std=c++20 -pthread

# ----------------------------------------------------------------------------
# 2. 极致严苛警告选项 (The Gauntlet)
# ----------------------------------------------------------------------------
# 基础全量警告并视为错误
CXXFLAGS += -Wall -Wextra -Wpedantic -Werror

# 【类型与转换强校验】
CXXFLAGS += -Wold-style-cast -Wuseless-cast -Wdouble-promotion -Wcast-align -Wcast-qual

# 【面向对象与内存安全强校验】
CXXFLAGS += -Wnon-virtual-dtor -Woverloaded-virtual -Wnull-dereference -Wvla -Wdelete-incomplete

# 【逻辑漏洞与代码规范校验】
CXXFLAGS += -Wformat=2 -Wmissing-declarations -Wduplicated-cond -Wduplicated-branches -Wlogical-op

# ----------------------------------------------------------------------------
# 3. 【精准豁免】仅放松简单数据类型隐式转换
# ----------------------------------------------------------------------------
# 允许有符号/无符号整数比较 (size_t <-> int)
CXXFLAGS += -Wno-sign-compare
# 允许枚举类型与普通整型隐式转换
CXXFLAGS += -Wno-enum-compare
# 允许基础整型隐式转换
CXXFLAGS += -Wno-implicit-int-conversion
# 允许三目运算符两侧类型不匹配
CXXFLAGS += -Wno-conditional-type-mismatch
# 允许成员与参数重名
CXXFLAGS += -Wshadow=local

# ----------------------------------------------------------------------------
# 4. 依赖管理与链接
# ----------------------------------------------------------------------------
CXXFLAGS += -MMD -MP
# 添加第三方库头文件路径
CXXFLAGS += -I/usr/local/include
CXXFLAGS += -I/usr/include/mavsdk

# 【重大修改】：将 -lmysqlclient 替换为 -lmysqlcppconn，以支持 C++ Connector
LDFLAGS  := -pthread -lmysqlcppconn -lmavsdk -lredis++ -lhiredis

# ----------------------------------------------------------------------------
# 5. 目录与文件自动发现
# ----------------------------------------------------------------------------
# 【重大修改】：确保加入了 user 和 redis_pool 目录
SRC_DIRS := . log connection_pool thread_pool timer_manager epoll_manager http_conn router listen work_queue redis_pool user
SRCS     := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.cpp))
OBJS     := $(patsubst %.cpp, %.o, $(SRCS))
DEPS     := $(OBJS:.o=.d)

# ----------------------------------------------------------------------------
# 6. 构建模式
# ----------------------------------------------------------------------------
.PHONY: all clean release debug run stop

all: release

release: CXXFLAGS += -O3 -flto -DNDEBUG
release: LDFLAGS  += -flto
release: $(TARGET)

debug: CXXFLAGS += -g3 -O0 -fsanitize=address,undefined -D_GLIBCXX_DEBUG
debug: LDFLAGS  += -fsanitize=address,undefined
debug: $(TARGET)

# ----------------------------------------------------------------------------
# 7. 核心增量编译规则（无修改，原生支持单文件编译）
# ----------------------------------------------------------------------------
$(TARGET): $(OBJS)
	@echo "[LINK] 链接所有目标文件"
	@$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# 模式规则：仅当 .cpp 比 .o 新时，才编译这一个文件
%.o: %.cpp
	@echo "[CXX]  仅编译修改的文件：$<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# 自动包含头文件依赖：修改 .h 也只触发关联 .cpp 编译
-include $(DEPS)

# ----------------------------------------------------------------------------
# 8. 清理规则
# ----------------------------------------------------------------------------
clean:
	@echo "[CLEAN] 删除编译产物..."
	@rm -f $(OBJS) $(DEPS) $(TARGET)

# ----------------------------------------------------------------------------
# 9. 自动化运行
# ----------------------------------------------------------------------------
# 根据你的代码上下文，默认端口设置为了 9006
PORT ?= 8080

run: all
	@echo "[PRE-CHECK] 检查端口 $(PORT) 占用..."
	@fuser -k $(PORT)/tcp > /dev/null 2>&1 || true
	@sleep 0.5
	@echo "[RUN] 启动服务器..."
	@./$(TARGET)

stop:
	@echo "[STOP] 关闭端口 $(PORT) 进程..."
	@fuser -k $(PORT)/tcp > /dev/null 2>&1 || true