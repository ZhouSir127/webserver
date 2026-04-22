# ============================================================================
# Top-Tier Strict C++ WebServer Makefile
# 仅豁免 int / size_t 类型转换警告，其余所有严苛校验完整保留
# ============================================================================

CXX      := g++
TARGET   := server

# ----------------------------------------------------------------------------
# 1. 核心标准与架构
# ----------------------------------------------------------------------------
CXXFLAGS := -std=c++17 -pthread

# ----------------------------------------------------------------------------
# 2. 极致严苛警告选项 (The Gauntlet)
# ----------------------------------------------------------------------------
# 基础全量警告并视为错误
CXXFLAGS += -Wall -Wextra -Wpedantic -Werror

# 【类型与转换强校验】（仅移除 int/size_t 转换相关警告，其余全部保留）
# 禁止 C 风格强转，强制使用 static_cast / reinterpret_cast
CXXFLAGS += -Wold-style-cast
# 禁止无意义的类型转换
CXXFLAGS += -Wuseless-cast
# 隐式 double 提升警告
CXXFLAGS += -Wdouble-promotion
# 指针强转对齐警告
CXXFLAGS += -Wcast-align
# 去除 const 修饰符警告
CXXFLAGS += -Wcast-qual

# 【面向对象与内存安全强校验】
# 多态基类必须有虚析构函数
CXXFLAGS += -Wnon-virtual-dtor
# 防止子类隐藏父类的重载虚函数
CXXFLAGS += -Woverloaded-virtual
# 捕获空指针解引用
CXXFLAGS += -Wnull-dereference
# 彻底禁止 C99 变长数组 (VLA)，要求必须使用 std::vector
CXXFLAGS += -Wvla
# 删除不完整类型的指针时报错
CXXFLAGS += -Wdelete-incomplete

# 【逻辑漏洞与代码规范校验】
# 格式化字符串强校验 (如 printf 系列)
CXXFLAGS += -Wformat=2
# 强制要求所有非静态函数在定义前必须有声明 (防止无意暴露全局符号)
CXXFLAGS += -Wmissing-declarations
# if-else 链中出现重复条件时警告
CXXFLAGS += -Wduplicated-cond
# if-else 分支执行完全相同代码时警告
CXXFLAGS += -Wduplicated-branches
# 逻辑运算符两边出现相同表达式时警告 (如 a == b && a == b)
CXXFLAGS += -Wlogical-op

# ----------------------------------------------------------------------------
# 3. 【精准豁免项】：允许成员与参数重名
# ----------------------------------------------------------------------------
CXXFLAGS += -Wshadow=local

# ----------------------------------------------------------------------------
# 4. 依赖管理与链接
# ----------------------------------------------------------------------------
CXXFLAGS += -MMD -MP

# MAVSDK 头文件路径
CXXFLAGS += -I/usr/include/mavsdk

# 链接库
# 使用 pkg-config 自动获取该版本 MAVSDK 实际需要的链接标志
LDFLAGS  := -pthread -lmysqlclient -lmavsdk

# ----------------------------------------------------------------------------
# 5. 目录与文件自动发现
# ----------------------------------------------------------------------------
SRC_DIRS := . log connection_pool thread_pool timer_manager epoll_manager http_conn router
SRCS     := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.cpp))
OBJS     := $(patsubst %.cpp, %.o, $(SRCS))
DEPS     := $(OBJS:.o=.d)

# ----------------------------------------------------------------------------
# 6. 构建模式 (Debug / Release)
# ----------------------------------------------------------------------------
.PHONY: all clean release debug

# 默认构建 Release 版本
all: release

# Release 模式
release: CXXFLAGS += -O3 -flto -DNDEBUG
release: LDFLAGS  += -flto
release: $(TARGET)

# Debug 模式
debug: CXXFLAGS += -g3 -O0 -fsanitize=address,undefined -D_GLIBCXX_DEBUG
debug: LDFLAGS  += -fsanitize=address,undefined
debug: $(TARGET)

# ----------------------------------------------------------------------------
# 7. 编译规则
# ----------------------------------------------------------------------------
$(TARGET): $(OBJS)
	@echo "[LINK] $@"
	@$(CXX) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.cpp
	@echo "[CXX]  $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	@echo "[CLEAN] Removing object files and executables..."
	@rm -f $(OBJS) $(DEPS) $(TARGET)

# ----------------------------------------------------------------------------
# 8. 自动化运行与端口清理 (Run & Auto-Kill)
# ----------------------------------------------------------------------------
# 定义服务器运行端口（需与 main.cpp 或 config.h 中配置的一致）
# 使用 ?= 允许你在命令行临时更改，如: make run PORT=8080
PORT ?= 8080

.PHONY: run stop

# 运行目标：先执行 build，再清理端口，最后启动
run: all
	@echo "[PRE-CHECK] Checking for existing process on port $(PORT)..."
	@# fuser -k 会向占用该端口的进程发送 SIGKILL 信号
	@# || true 保证即使没有进程占用，make 也会继续执行而不报错
	@fuser -k $(PORT)/tcp > /dev/null 2>&1 || true
	@sleep 0.5
	@echo "[RUN] Starting $(TARGET) on port $(PORT)..."
	@./$(TARGET)

# 单独停止目标的指令
stop:
	@echo "[STOP] Killing process on port $(PORT)..."
	@fuser -k $(PORT)/tcp > /dev/null 2>&1 || true
