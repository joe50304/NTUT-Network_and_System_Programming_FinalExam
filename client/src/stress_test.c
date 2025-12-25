#define _POSIX_C_SOURCE 200809L // 為了 clock_gettime
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include "client_core.h"
#include "stress_test.h"
#include "protocol.h"

// ANSI Colors
#define COLOR_CYAN  "\033[36m"
#define COLOR_GREEN "\033[32m"
#define COLOR_RED   "\033[31m"
#define COLOR_RESET "\033[0m"

// 全域同步變數 (起跑線)
pthread_barrier_t barrier;

// 每個執行緒的參數
typedef struct {
    int thread_id;
    const char *ip;
    int port;
    int requests;
    unsigned int rand_seed; // 每個執行緒獨立的亂數種子
} ThreadArgs;

// 每個執行緒的統計數據 (避免鎖競爭)
typedef struct {
    long success_count;
    long fail_count;
    double total_latency_ms; // 累積延遲 (毫秒)
} ThreadStats;

// 統計陣列 (最後加總用)
ThreadStats *all_stats;

// 時間計算小工具
double get_time_diff_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + 
           (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

// ==========================================
// Worker Thread (模擬單一客戶)
// ==========================================
void *worker_routine(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    int tid = args->thread_id;
    ClientContext ctx;
    
    // 1. 初始化並連線
    // 注意：壓力測試通常不需要 Strict 驗證，為了速度可設為 0
    client_init(&ctx, 0); 

    // 連線失敗直接退出，不計入 Barrier (或視需求調整)
    if (client_connect(&ctx, args->ip, args->port) < 0) {
        fprintf(stderr, "Thread %d failed to connect.\n", tid);
        all_stats[tid].fail_count = args->requests; // 全部視為失敗
        pthread_barrier_wait(&barrier); // 還是要等，避免卡死其他執行緒
        return NULL;
    }
    
    // --- 新增：自動化 OTP 登入流程 ---
    char my_account[20];
    snprintf(my_account, sizeof(my_account), "USER_%d", tid);
    
    // 1. 請求 OTP
    OtpRequest otp_req;
    strncpy(otp_req.account_id, my_account, sizeof(otp_req.account_id));
    
    PacketHeader header_out;
    char recv_buf[1024];
    
    // 發送請求
    if (client_send(&ctx, OP_REQ_OTP, &otp_req, sizeof(otp_req)) > 0) {
        // 接收回應
        if (client_receive(&ctx, &header_out, recv_buf, sizeof(recv_buf)) >= 0) {
            BankingResponse *resp = (BankingResponse *)recv_buf;
            
            if (resp->status == 0) { // 假設 0 是 STATUS_SUCCESS
                // 2. 登入 (Server 把 OTP 放在 message 裡回傳)
                LoginRequest login_req;
                strncpy(login_req.account_id, my_account, sizeof(login_req.account_id));
                strncpy(login_req.otp, resp->message, sizeof(login_req.otp)); 
                
                client_send(&ctx, OP_LOGIN, &login_req, sizeof(login_req));
                client_receive(&ctx, &header_out, recv_buf, sizeof(recv_buf));
                // 這裡可以加判斷是否登入成功
            }
        }
    }


    // 2. 等待起跑信號 (確保所有連線都建立好了才開始攻擊)
    pthread_barrier_wait(&barrier);

    // 3. 開始迴圈發送請求
    struct timespec start_ts, end_ts;


    for (int i = 0; i < args->requests; i++) {
        // 隨機產生交易 (存款/提款/查餘額)
        int action = rand_r(&args->rand_seed) % 3;
        uint16_t op;
        // Use a union or largest struct to hold request data
        union {
            DepositRequest dep;
            WithdrawRequest wid;
            BalanceRequest bal;
        } trans_req;
        
        switch (action) {
            case 0: op = OP_DEPOSIT; break;
            case 1: op = OP_WITHDRAW; break;
            default: op = OP_BALANCE; break;
        }

        if (op == OP_DEPOSIT || op == OP_WITHDRAW) {
             DepositRequest *req = (DepositRequest *)&trans_req; // Cast to reuse
             snprintf(req->account_id, sizeof(req->account_id), "%d", tid);
             req->amount = 10;
        } else if (op == OP_BALANCE) {
             BalanceRequest *req = (BalanceRequest *)&trans_req;
             snprintf(req->account_id, sizeof(req->account_id), "%d", tid);
        }

        // --- 計時開始 ---
        clock_gettime(CLOCK_MONOTONIC, &start_ts);

        // 發送
        if (client_send(&ctx, op, &trans_req, sizeof(trans_req)) > 0) {
            // 接收
            if (client_receive(&ctx, &header_out, recv_buf, sizeof(recv_buf)) >= 0) {
                // --- 計時結束 ---
                clock_gettime(CLOCK_MONOTONIC, &end_ts);
                
                all_stats[tid].success_count++;
                all_stats[tid].total_latency_ms += get_time_diff_ms(start_ts, end_ts);
            } else {
                all_stats[tid].fail_count++;
            }
        } else {
            all_stats[tid].fail_count++;
        }
    }

    client_close(&ctx);
    return NULL;
}

// ==========================================
// 主控台 (Manager)
// ==========================================
void run_stress_test(const char *ip, int port, int num_threads, int num_requests) {
    printf(COLOR_CYAN "=== Stress Test Started ===\n");
    printf("Target: %s:%d\n", ip, port);
    printf("Threads: %d, Requests/Thread: %d\n", num_threads, num_requests);
    printf("Total Requests: %d\n" COLOR_RESET, num_threads * num_requests);

    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
    ThreadArgs *args = malloc(sizeof(ThreadArgs) * num_threads);
    all_stats = calloc(num_threads, sizeof(ThreadStats)); // 使用 calloc 初始化為 0

    // 初始化 Barrier (等待 N 個線程)
    pthread_barrier_init(&barrier, NULL, num_threads);

    struct timespec global_start, global_end;

    // 1. 建立執行緒
    printf("[*] Spawning threads...\n");
    for (int i = 0; i < num_threads; i++) {
        args[i].thread_id = i;
        args[i].ip = ip;
        args[i].port = port;
        args[i].requests = num_requests;
        args[i].rand_seed = time(NULL) + i; // 確保隨機種子不同
        pthread_create(&threads[i], NULL, worker_routine, &args[i]);
    }

    // 記錄整體開始時間 (注意：這裡的開始時間包含連線建立過程，
    // 若要精確算 TPS，可以把計時移到 barrier 之後，但 client 端通常算總時間)
    clock_gettime(CLOCK_MONOTONIC, &global_start);

    // 2. 等待所有執行緒結束
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &global_end);
    printf(COLOR_GREEN "[v] All threads finished.\n" COLOR_RESET);

    // 3. 統計與報告
    long total_success = 0;
    long total_fail = 0;
    double total_latency_sum = 0.0;

    for (int i = 0; i < num_threads; i++) {
        total_success += all_stats[i].success_count;
        total_fail += all_stats[i].fail_count;
        total_latency_sum += all_stats[i].total_latency_ms;
    }

    double total_time_ms = get_time_diff_ms(global_start, global_end);
    double avg_latency = (total_success > 0) ? (total_latency_sum / total_success) : 0.0;
    double throughput = (total_success / (total_time_ms / 1000.0)); // Req per second

    printf("\n" COLOR_CYAN "=== Test Report ===" COLOR_RESET "\n");
    printf("Total Time    : %.2f ms\n", total_time_ms);
    printf("Total Requests: %ld (Success: %ld, Fail: %ld)\n", 
           total_success + total_fail, total_success, total_fail);
    printf("Avg Latency   : %.3f ms\n", avg_latency);
    printf("Throughput    : " COLOR_GREEN "%.2f TPS" COLOR_RESET " (Transactions Per Second)\n", throughput);
    printf("===================\n");

    // 清理資源
    pthread_barrier_destroy(&barrier);
    free(threads);
    free(args);
    free(all_stats);
}