#ifndef CLIENT_CORE_H
#define CLIENT_CORE_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>
#include "protocol.h"

// 定義一個結構來管理連線狀態，避免全域變數
typedef struct {
    int socket_fd;
    SSL_CTX *ctx;       // OpenSSL 上下文
    SSL *ssl;           // 每個連線的 SSL 結構
    struct sockaddr_in server_addr;
    int is_connected;
} ClientContext;

// ==========================================
// 核心功能函式
// ==========================================

/**
 * 初始化 Client Context (載入憑證、設定 SSL ctx)
 * verify_server: 1 = 開啟 Server 憑證驗證, 0 = 關閉
 */
void client_init(ClientContext *client, int verify_server);

/**
 * 建立 TCP 連線並執行 TLS Handshake
 * ip: Server IP
 * port: Server Port
 * return: 0 成功, -1 失敗
 */
int client_connect(ClientContext *client, const char *ip, int port);

/**
 * 發送請求 (自動封裝 Header + Body 並加密傳送)
 * op_code: 操作碼 (如 OP_LOGIN)
 * payload: 封包內容 (可以是 LoginRequest, TransactionRequest 等)
 * payload_len: 內容長度
 * return: 發送的 bytes 數, -1 失敗
 */
int client_send(ClientContext *client, uint16_t op_code, void *payload, uint32_t payload_len);

/**
 * 接收回應 (解密並解析 Header)
 * header_out: (輸出) 接收到的 Header 資訊
 * body_buffer: (輸出) 接收到的 Body 內容
 * buffer_size: buffer 大小
 * return: 接收到的 Body 長度, -1 失敗
 */
int client_receive(ClientContext *client, PacketHeader *header_out, void *body_buffer, uint32_t buffer_size);

/**
 * 斷線並釋放資源
 */
void client_close(ClientContext *client);

#endif // CLIENT_CORE_H