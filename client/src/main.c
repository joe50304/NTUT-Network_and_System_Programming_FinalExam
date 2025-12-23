#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client_core.h"
#include "protocol.h"
#include "stress_test.h" // 加入這行

int main(int argc, char *argv[]) {
    // 參數格式: <ip> <port> <mode> [params...]
    // Mode 1: Single Test -> <ip> <port> 1
    // Mode 2: Stress Test -> <ip> <port> 2 <threads> <requests>

    if (argc < 4) {
        printf("Usage:\n");
        printf("  Single Test: %s <ip> <port> 1\n", argv[0]);
        printf("  Stress Test: %s <ip> <port> 2 <threads> <requests>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int mode = atoi(argv[3]);

    if (mode == 1) {
        // === 單次測試模式 (原本的代碼) ===
        printf("Running Single Connection Test...\n");
        ClientContext ctx;
        client_init(&ctx, 0); // 測試模式暫關閉驗證

        if (client_connect(&ctx, ip, port) < 0) {
            fprintf(stderr, "Connection failed.\n");
            return EXIT_FAILURE;
        }
        printf("Connected! Sending login...\n");

        LoginRequest login_req = {"user", "pass"};
        client_send(&ctx, OP_LOGIN, &login_req, sizeof(login_req));
        
        // 嘗試接收 (可能會 timeout)
        PacketHeader header;
        char buf[1024];
        if (client_receive(&ctx, &header, buf, sizeof(buf)) > 0) {
            printf("Received response!\n");
        }
        
        client_close(&ctx);

    } else if (mode == 2) {
        // === 壓力測試模式 ===
        if (argc != 6) {
            printf("Error: Stress test needs thread count and request count.\n");
            return EXIT_FAILURE;
        }
        int threads = atoi(argv[4]);
        int reqs = atoi(argv[5]);
        
        run_stress_test(ip, port, threads, reqs);
    }

    return EXIT_SUCCESS;
}