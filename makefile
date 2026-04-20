# ==========================================
# WebServer 工业级极严苛 Makefile (C++23)
# ==========================================

CXX      := g++
TARGET   := server

# ------------------------------------------
# 极严苛警告标志 (The Ultimate Strict Flags)
# ------------------------------------------
# 基础与扩展
WARNINGS := -Wall -Wextra -Werror -Wpedantic

# 内存、指针与类型安全
# -Wnull-dereference: 检测可能的空指针解引用
# -Wpointer-arith: 禁止对 void* 进行非法的指针算术运算
# -Wcast-align: 检测可能导致架构崩溃的内存对齐转换问题
# -Wcast-qual: 严禁将 const 强转为非 const (破坏常量正确性)
WARNINGS += -Wnull-dereference -Wpointer-arith -Wcast-align -Wcast-qual
WARNINGS += -Wundef -Wvla -Wredundant-decls

# 逻辑与条件判定
# -Wswitch-enum: switch 语句必须覆盖 enum 的所有枚举值
WARNINGS += -Wlogical-op -Wduplicated-cond -Wduplicated-branches -Wmisleading-indentation
WARNINGS += -Wswitch-default -Wswitch-enum

# 隐式转换与强制转换
WARNINGS += -Wconversion -Wdouble-promotion -Wuseless-cast -Wold-style-cast -Wsign-promo

# C++ 面向对象专属
# -Woverloaded-virtual: 防止派生类意外隐藏基类的虚函数
# -Wnon-virtual-dtor: 带虚函数的类必须拥有虚析构函数 (防止多态内存泄漏)
# -Winit-self: 检查类成员是否用自己初始化了自己
WARNINGS += -Woverloaded-virtual -Wnon-virtual-dtor -Winit-self

# 格式化字符串安全
WARNINGS += -Wformat=2 -Wformat-security

# 【注】：为了允许 expire(expire) 这种写法，已明确移除 -Wshadow 选项。

# ------------------------------------------
# 编译参数
# ------------------------------------------
CXXFLAGS := -std=c++23 $(WARNINGS) -I/usr/include/mavsdk

# ------------------------------------------
# 安全防护机制 (Security Hardening)
# ------------------------------------------
# 栈保护与缓冲区溢出检测 (防止黑客通过溢出攻击你的服务器)
SECURITY_FLAGS := -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE

# ------------------------------------------
# 调试与优化配置
# ------------------------------------------
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    # 开发模式：开启 ASan/UBSan 动态内存追踪，STL 边界检查
    SANITIZERS := -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
    CXXFLAGS += -g -O0 -D_GLIBCXX_ASSERTIONS $(SANITIZERS) $(SECURITY_FLAGS)
    LDFLAGS  := $(SANITIZERS) -pie
else
    # 生产模式：极限性能优化，开启 RELRO 内存只读保护
    CXXFLAGS += -O3 -DNDEBUG $(SECURITY_FLAGS)
    LDFLAGS  := -pie -Wl,-z,relro,-z,now
endif

# ------------------------------------------
# 库依赖
# ------------------------------------------
LDFLAGS += -L/usr/lib/x86_64-linux-gnu -L/usr/local/lib
LIBS    := -pthread -lmysqlclient -lmavsdk

# ------------------------------------------
# 源文件与目标文件
# ------------------------------------------
# ------------------------------------------
# 源文件与目标文件 (修正后的版本)
# ------------------------------------------
SRCS    := main.cpp \
           webserver.cpp \
           http_conn/http_conn.cpp \
           thread_pool/thread_pool.cpp \
           timer_manager/timer_manager.cpp \
           epoll_manager/epoll_manager.cpp \
           log/log.cpp \
           connection_pool/connection_pool.cpp  # 修正此处

OBJS    := $(SRCS:.cpp=.o)
DEPS    := $(SRCS:.cpp=.d)

# ------------------------------------------
# 构建规则
# ------------------------------------------
.PHONY: all clean rebuild run

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "🔗 Linking executable [$(TARGET)]..."
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LIBS)
	@echo "✅ Build completed successfully!"

%.o: %.cpp
	@echo "🔨 Compiling $<..."
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

# ------------------------------------------
# 辅助指令
# ------------------------------------------
clean:
	@echo "🧹 Cleaning up build files..."
	rm -f $(TARGET) $(OBJS) $(DEPS)

rebuild: clean all

run: $(TARGET)
	@echo "\n[1/2] 正在释放端口 8080..."
	-@fuser -k 8080/tcp 2>/dev/null || true
	@echo "[2/2] 🚀 启动 $(TARGET) (开启内存监控)...\n"
	./$(TARGET)