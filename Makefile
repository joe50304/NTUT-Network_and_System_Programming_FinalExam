# ==========================================
# Banking System - Integrated Makefile
# Server (你) + Client (組員) 整合版
# ==========================================

CC = gcc
CFLAGS = -Wall -g -pthread -I./common/include -I./client/include -I./common
LIBS = -lssl -lcrypto -pthread
LDFLAGS = $(LIBS)

# ==========================================
# 目錄設定
# ==========================================
BIN_DIR = bin
OBJ_DIR = obj

# Server 目錄（你的部分）
SERVER_SRC_DIR = server
SERVER_OBJ_DIR = $(OBJ_DIR)/server

# Client 目錄（組員的部分）
CLIENT_SRC_DIR = client
CLIENT_OBJ_DIR = $(OBJ_DIR)/client

# Stress Test 目錄
STRESS_SRC_DIR = stress_test
STRESS_OBJ_DIR = $(OBJ_DIR)/stress_test

# Common 目錄（共用）
# 支援兩種結構：
# 1. common/src/*.c (組員的結構)
# 2. common/*.c (你的結構)
COMMON_SRC_DIR_1 = common/src
COMMON_SRC_DIR_2 = common
COMMON_OBJ_DIR_1 = $(OBJ_DIR)/common/src
COMMON_OBJ_DIR_2 = $(OBJ_DIR)/common

# ==========================================
# 原始碼搜尋
# ==========================================
# Server 源碼（你的）
SERVER_SRCS = $(wildcard $(SERVER_SRC_DIR)/*.c)
SERVER_OBJS = $(patsubst $(SERVER_SRC_DIR)/%.c, $(SERVER_OBJ_DIR)/%.o, $(SERVER_SRCS))

# Client 源碼（組員的）
CLIENT_SRCS = $(CLIENT_SRC_DIR)/banking_client.c
CLIENT_OBJS = $(patsubst $(CLIENT_SRC_DIR)/%.c, $(CLIENT_OBJ_DIR)/%.o, $(CLIENT_SRCS))

# Common 源碼（共用，支援兩種結構）
COMMON_SRCS_1 = $(wildcard $(COMMON_SRC_DIR_1)/*.c)
COMMON_SRCS_2 = $(wildcard $(COMMON_SRC_DIR_2)/*.c)
COMMON_OBJS_1 = $(patsubst $(COMMON_SRC_DIR_1)/%.c, $(COMMON_OBJ_DIR_1)/%.o, $(COMMON_SRCS_1))
COMMON_OBJS_2 = $(patsubst $(COMMON_SRC_DIR_2)/%.c, $(COMMON_OBJ_DIR_2)/%.o, $(COMMON_SRCS_2))

# 合併所有 Common objects
COMMON_OBJS = $(COMMON_OBJS_1) $(COMMON_OBJS_2)

# ==========================================
# 目標執行檔
# ==========================================
SERVER_TARGET = $(BIN_DIR)/banking_server
CLIENT_TARGET = $(BIN_DIR)/banking_client
OTP_TARGET = $(BIN_DIR)/otp_server
STRESS_TARGET = $(BIN_DIR)/stress_client
COMMON_LIB = $(BIN_DIR)/libcommon.a
# ==========================================
# 主要規則
# ==========================================
.PHONY: all server client directories clean clean-ipc help

# 預設：編譯 Server 和 Client
all: directories $(COMMON_LIB) server client otp stress

# 只編譯 Server（你的部分）
server: directories $(SERVER_TARGET)
	@echo "✅ Server compiled successfully!"
	@echo "Run: ./$(SERVER_TARGET) 8888 0"

# 只編譯 Client（組員的部分）
client: directories $(COMMON_LIB) $(CLIENT_TARGET)
	@echo "✅ Client compiled successfully!"
	@echo "Run: ./$(CLIENT_TARGET) localhost 8888 0"

# 只編譯 Stress Client
stress: directories $(COMMON_LIB) $(STRESS_TARGET)
	@echo "✅ Stress Client compiled successfully!"
	@echo "Run: ./$(STRESS_TARGET) 127.0.0.1 8888 100 100 0"

# 只編譯 OTP Server
otp: directories $(OTP_TARGET)
	@echo "✅ OTP Server compiled successfully!"

# 建立必要目錄
directories:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(SERVER_OBJ_DIR)
	@mkdir -p $(CLIENT_OBJ_DIR)
	@mkdir -p $(STRESS_OBJ_DIR)
	@mkdir -p $(COMMON_OBJ_DIR_1)
	@mkdir -p $(COMMON_OBJ_DIR_2)

# ==========================================
# Server 編譯規則（你的部分）
# ==========================================
$(SERVER_TARGET): $(SERVER_OBJS) $(COMMON_LIB)
	@echo "🔗 Linking Server Application..."
	$(CC) $(SERVER_OBJS) -L$(BIN_DIR) -lcommon -o $@ $(LDFLAGS)
	@echo "Server binary created: $@"

$(SERVER_OBJ_DIR)/%.o: $(SERVER_SRC_DIR)/%.c
	@echo "📝 Compiling Server: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# ==========================================
# Client 編譯規則（組員的部分）
# ==========================================
$(CLIENT_TARGET): $(CLIENT_OBJS) $(COMMON_LIB)
	@echo "🔗 Linking Client Application..."
	$(CC) $(CLIENT_OBJS) -L$(BIN_DIR) -lcommon -o $@ $(LDFLAGS)
	@echo "Client binary created: $@"

$(CLIENT_OBJ_DIR)/%.o: $(CLIENT_SRC_DIR)/%.c
	@echo "📝 Compiling Client: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# ==========================================
# OTP 編譯規則
# ==========================================

$(OTP_TARGET): otp_server/otp_server.c
	$(CC) $(CFLAGS) otp_server/otp_server.c -o $@

# ==========================================
# Stress Client 編譯規則
# ==========================================
$(STRESS_TARGET): $(OBJ_DIR)/stress_test/stress_client.o $(COMMON_LIB)
	@echo "🔗 Linking Stress Client..."
	$(CC) $(OBJ_DIR)/stress_test/stress_client.o -L$(BIN_DIR) -lcommon -o $@ $(LDFLAGS)
	@echo "Stress Client binary created: $@"

$(OBJ_DIR)/stress_test/stress_client.o: $(STRESS_SRC_DIR)/stress_client.c
	@echo "📝 Compiling Stress Client: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# ==========================================
# Common 編譯規則（共用模組） - 靜態函式庫
# ==========================================
$(COMMON_LIB): $(COMMON_OBJS)
	@echo "📚 Archiving Common Library..."
	ar rcs $@ $^
	@echo "Static Library created: $@"

# 支援 common/src/*.c (組員的結構)
$(COMMON_OBJ_DIR_1)/%.o: $(COMMON_SRC_DIR_1)/%.c
	@echo "📚 Compiling Common (src): $<"
	$(CC) $(CFLAGS) -c $< -o $@

# 支援 common/*.c (你的結構)
$(COMMON_OBJ_DIR_2)/%.o: $(COMMON_SRC_DIR_2)/%.c
	@echo "📚 Compiling Common: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# ==========================================
# 清理功能
# ==========================================
clean:
	@echo "🧹 Cleaning up build artifacts..."
	rm -rf $(BIN_DIR) $(OBJ_DIR)
	rm -f *.log *.out core *~
	@echo "Clean complete."

# 清理 IPC 資源（Server 異常終止時使用）
clean-ipc:
	@echo "🧹 Cleaning IPC resources..."
	@ipcs -m | grep 0x12345678 | awk '{print $$2}' | xargs -r ipcrm -m
	@echo "IPC resources cleaned."

# ==========================================
# 幫助資訊
# ==========================================
help:
	@echo "=========================================="
	@echo "Banking System Makefile"
	@echo "=========================================="
	@echo "Targets:"
	@echo "  make              - Build both server and client"
	@echo "  make server       - Build server only"
	@echo "  make client       - Build client only"
	@echo "  make clean        - Remove all build artifacts"
	@echo "  make clean-ipc    - Clean IPC shared memory"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  Server: ./$(SERVER_TARGET) <port> <verify_client>"
	@echo "          Example: ./$(SERVER_TARGET) 8888 0"
	@echo ""
	@echo "  Client: ./$(CLIENT_TARGET) <host> <port> <verify_server>"
	@echo "          Example: ./$(CLIENT_TARGET) localhost 8888 0"
	@echo "=========================================="
