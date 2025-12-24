#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "client_core.h"
#include "crypto.h"

#define CA_CERT_PATH "certificate/ca.crt"
#define CLIENT_CERT_PATH "certificate/client.crt"
#define CLIENT_KEY_PATH "certificate/client.key"

// 內部 Helper: 錯誤處理
static void handle_ssl_error(const char *msg) {
    perror(msg);
    ERR_print_errors_fp(stderr);
}

// 1. 初始化 (對應 secure_client.c 的 create_context & configure_context)
void client_init(ClientContext *client, int verify_server) {
    memset(client, 0, sizeof(ClientContext));

    // 建立 SSL Context
    const SSL_METHOD *method = TLS_client_method();
    client->ctx = SSL_CTX_new(method);
    if (!client->ctx) {
        handle_ssl_error("Unable to create SSL context");
        exit(EXIT_FAILURE);
    }

    // 載入 CA 憑證 (驗證 Server 用)
    if (SSL_CTX_load_verify_locations(client->ctx, CA_CERT_PATH, NULL) <= 0) {
        fprintf(stderr, "[Warning] Failed to load CA certificate. Verify might fail.\n");
    }

    // 載入 Client 憑證 (雙向驗證 mTLS 用，選用)
    if (SSL_CTX_use_certificate_file(client->ctx, CLIENT_CERT_PATH, SSL_FILETYPE_PEM) > 0 &&
        SSL_CTX_use_PrivateKey_file(client->ctx, CLIENT_KEY_PATH, SSL_FILETYPE_PEM) > 0) {
        // printf("[Info] Client certificate loaded.\n");
    }

    // 設定驗證模式
    if (verify_server) {
        SSL_CTX_set_verify(client->ctx, SSL_VERIFY_PEER, NULL);
    } else {
        SSL_CTX_set_verify(client->ctx, SSL_VERIFY_NONE, NULL);
    }
}

// 2. 連線 (TCP Connect + SSL Handshake)
int client_connect(ClientContext *client, const char *ip, int port) {
    // 建立 TCP Socket
    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &client->server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        return -1;
    }

    // TCP 連線
    if (connect(client->socket_fd, (struct sockaddr *)&client->server_addr, sizeof(client->server_addr)) < 0) {
        perror("Connection failed");
        close(client->socket_fd);
        return -1;
    }

    // 建立 SSL 物件並綁定 Socket
    client->ssl = SSL_new(client->ctx);
    SSL_set_fd(client->ssl, client->socket_fd);
    
    // 設定 SNI (Optional, 但現代 TLS 建議要有)
    SSL_set_tlsext_host_name(client->ssl, "banking.system");

    // 執行 TLS Handshake
    if (SSL_connect(client->ssl) <= 0) {
        handle_ssl_error("TLS Handshake failed");
        client_close(client);
        return -1;
    }

    // 檢查憑證驗證結果
    if (SSL_get_verify_mode(client->ssl) == SSL_VERIFY_PEER) {
        if (SSL_get_verify_result(client->ssl) != X509_V_OK) {
            fprintf(stderr, "[Security] Certificate Verification Failed!\n");
            client_close(client);
            return -1;
        }
    }

    client->is_connected = 1;
    return 0;
}

// 3. 發送請求 (已加入安全性 Hook)
int client_send(ClientContext *client, uint16_t op_code, void *payload, uint32_t payload_len) {
    if (!client->is_connected || !client->ssl) return -1;

    PacketHeader header;
    header.length = sizeof(PacketHeader) + payload_len;
    header.op_code = op_code;
    header.req_id = rand(); 
    
    // [Security Hook 1] 計算 Checksum
    // 這裡我們計算 payload 的校驗碼放入 Header
    // 如果沒有 Payload，Checksum 為 0
    if (payload && payload_len > 0) {
        header.checksum = calculate_checksum(payload, payload_len);
    } else {
        header.checksum = 0;
    }

    char buffer[4096];
    if (header.length > sizeof(buffer)) {
        fprintf(stderr, "Packet too large!\n");
        return -1;
    }

    // 複製 Header
    memcpy(buffer, &header, sizeof(PacketHeader));
    
    // 複製 Body
    if (payload && payload_len > 0) {
        memcpy(buffer + sizeof(PacketHeader), payload, payload_len);
    }

    int bytes_sent = SSL_write(client->ssl, buffer, header.length);
    if (bytes_sent <= 0) {
        // handle_ssl_error("SSL Write failed"); // 暫時註解避免干擾輸出
        return -1;
    }

    return bytes_sent;
}

// 4. 接收回應 (已加入安全性 Hook)
int client_receive(ClientContext *client, PacketHeader *header_out, void *body_buffer, uint32_t buffer_size) {
    if (!client->is_connected || !client->ssl) return -1;

    // 1. 讀取 Header
    int bytes = SSL_read(client->ssl, header_out, sizeof(PacketHeader));
    if (bytes <= 0) return -1;

    uint32_t body_len = header_out->length - sizeof(PacketHeader);
    
    // 2. 讀取 Body
    if (body_len > 0) {
        if (body_len > buffer_size) return -1; // Buffer 不夠大
        
        int body_read = SSL_read(client->ssl, body_buffer, body_len);
        if (body_read <= 0) return -1;

        // [Security Hook 2] 驗證 Checksum
        // Server 傳回來的資料，我們也要檢查有沒有壞掉
        if (!verify_checksum(header_out->checksum, body_buffer, body_len)) {
            fprintf(stderr, "[Security Alert] Checksum Mismatch! Data might be corrupted.\n");
            return -2; // 回傳特殊錯誤碼
        }
        
        return body_read;
    }

    return 0; 
}

// 5. 斷線與清理
void client_close(ClientContext *client) {
    if (client->ssl) {
        SSL_shutdown(client->ssl);
        SSL_free(client->ssl);
        client->ssl = NULL;
    }
    if (client->socket_fd > 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }
    if (client->ctx) {
        SSL_CTX_free(client->ctx);
        client->ctx = NULL;
    }
    client->is_connected = 0;
}