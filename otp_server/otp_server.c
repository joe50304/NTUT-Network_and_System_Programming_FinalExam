/*
 * otp_server.c
 * 專門負責生成與驗證 OTP 的微服務
 * 通訊協定：Raw TCP + Binary Struct (No HTTP)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include "../common/include/otp_ipc.h"

#define MAX_OTPS 100

// 簡單的內存資料庫
typedef struct {
    char account[32];
    char otp[8];
    time_t expiry;
    int used;
} OtpEntry;

OtpEntry otp_db[MAX_OTPS];

void handle_client(int client_fd) {
    OtpIpcRequest req;
    OtpIpcResponse res;
    memset(&res, 0, sizeof(res));

    // 1. 讀取請求 (直接讀取二進位 struct)
    int bytes = read(client_fd, &req, sizeof(req));
    if (bytes <= 0) return;

    printf("[OTP Server] Recv Op: %d, User: %s\n", req.op_code, req.account);

    if (req.op_code == OTP_OP_GENERATE) {
        // 生成 OTP
        int slot = -1;
        // 找空位或更新舊的
        for (int i = 0; i < MAX_OTPS; i++) {
            if (!otp_db[i].used || strcmp(otp_db[i].account, req.account) == 0) {
                slot = i;
                break;
            }
        }

        if (slot >= 0) {
            // 產生 6 位數亂數
            int code = rand() % 900000 + 100000;
            snprintf(otp_db[slot].otp, 8, "%d", code);
            strcpy(otp_db[slot].account, req.account);
            otp_db[slot].used = 1;
            otp_db[slot].expiry = time(NULL) + 300; // 5分鐘有效

            res.status = 1;
            strcpy(res.otp_code, otp_db[slot].otp);
            strcpy(res.message, "OTP Generated");
            printf("[OTP Server] Gen OTP for %s: %s\n", req.account, res.otp_code);
        } else {
            res.status = 0;
            strcpy(res.message, "Server Busy");
        }

    } else if (req.op_code == OTP_OP_VERIFY) {
        // 驗證 OTP
        int found = 0;
        for (int i = 0; i < MAX_OTPS; i++) {
            if (otp_db[i].used && 
                strcmp(otp_db[i].account, req.account) == 0) {
                
                if (strcmp(otp_db[i].otp, req.otp_code) == 0) {
                    found = 1;
                    otp_db[i].used = 0; // 用過即丟 (One-Time)
                }
                break;
            }
        }

        if (found) {
            res.status = 1;
            strcpy(res.message, "Verified");
            printf("[OTP Server] %s Verified Success\n", req.account);
        } else {
            res.status = 0;
            strcpy(res.message, "Invalid OTP");
            printf("[OTP Server] %s Verify Failed\n", req.account);
        }
    }

    // 2. 回傳回應
    write(client_fd, &res, sizeof(res));
}

int main() {
    srand(time(NULL));
    memset(otp_db, 0, sizeof(otp_db));

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(OTP_PORT);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    printf("=== C OTP Server Listening on Port %d ===\n", OTP_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &len);
        
        if (client_fd >= 0) {
            handle_client(client_fd);
            close(client_fd); // 短連線：處理完就斷開
        }
    }
    return 0;
}