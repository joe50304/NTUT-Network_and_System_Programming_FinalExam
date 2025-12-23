# ==========================================
# High-Concurrency Banking System Makefile
# ==========================================

# 1. 編譯器設定
CC = gcc
# 連結選項: 
# -lssl -lcrypto: OpenSSL 加密庫
# -pthread: 多執行緒支援
LIBS = -lssl -lcrypto -pthread

# 編譯選項:
# -Wall: 顯示所有警告
# -g: 產生除錯資訊 (GDB用)
# -I...: 指定 Header 檔搜尋路徑
CFLAGS = -Wall -g -pthread -I./common/include -I./client/include

LDFLAGS = $(LIBS)

# 2. 路徑設定
BIN_DIR = bin
OBJ_DIR = obj
CLIENT_SRC_DIR = client/src
COMMON_SRC_DIR = common/src

# 3. 自動搜尋原始碼
# wildcard 會抓取目錄下所有 .c 檔
CLIENT_SRCS = $(wildcard $(CLIENT_SRC_DIR)/*.c)
COMMON_SRCS = $(wildcard $(COMMON_SRC_DIR)/*.c)

# 4. 產生對應的 .o 檔名
# 例如: client/src/main.c -> obj/client/src/main.o
CLIENT_OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(CLIENT_SRCS))
COMMON_OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(COMMON_SRCS))

# 目標執行檔名稱
CLIENT_TARGET = $(BIN_DIR)/client_app

# ==========================================
# Rules (規則區)
# ==========================================

# 預設動作: 建立目錄 -> 編譯 Client
all: directories $(CLIENT_TARGET)

# 建立輸出資料夾
directories:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)/$(CLIENT_SRC_DIR)
	@mkdir -p $(OBJ_DIR)/$(COMMON_SRC_DIR)

# 連結 (Linking): 將所有 .o 檔結合成執行檔
$(CLIENT_TARGET): $(CLIENT_OBJS) $(COMMON_OBJS)
	@echo "Linking Client Application..."
	$(CC) $(CLIENT_OBJS) $(COMMON_OBJS) -o $@ $(LDFLAGS)
	@echo "Build Successful! Run with: ./$(CLIENT_TARGET)"

# 編譯 Client 源碼 (.c -> .o)
$(OBJ_DIR)/$(CLIENT_SRC_DIR)/%.o: $(CLIENT_SRC_DIR)/%.c
	@echo "Compiling Client: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# 編譯 Common 源碼 (.c -> .o)
$(OBJ_DIR)/$(COMMON_SRC_DIR)/%.o: $(COMMON_SRC_DIR)/%.c
	@echo "Compiling Common Lib: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# ==========================================
# Clean (清除功能)
# ==========================================
clean:
	@echo "Cleaning up build artifacts..."
	# 1. 刪除執行檔目錄 (bin) 和 中間檔目錄 (obj)
	rm -rf $(BIN_DIR) $(OBJ_DIR)
	
	@echo "Cleaning up temporary files..."
	# 2. 刪除可能會產生的 log 檔、core dump 檔 (程式崩潰紀錄)、以及編輯器暫存檔 (*~)
	rm -f *.log *.out core *~ 
	
	@echo "Clean complete."

# 偽目標宣告 (避免目錄下有同名檔案導致 make 誤判)
.PHONY: all clean directories