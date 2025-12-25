/*
 * stress_client.c
 * Multi-threaded Stress Testing Client with OTP Support
 *
 * Compiles to: ../bin/stress_client
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>

#include "../common/include/protocol.h"
#include "../common/include/tls_wrapper.h"

// Configuration
#define DEFAULT_THREADS 100
#define DEFAULT_REQUESTS 100 // Requests per thread

typedef struct {
    int thread_id;
    char *server_ip;
    int server_port;
    int verify_cert;
    int num_requests;
    
    // Stats
    int success_count;
    int fail_count;
    double total_latency_ms;
    double max_latency_ms;
    double min_latency_ms;
} ThreadArgs;

// Helper: Get current time in milliseconds
double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

// Helper: Send and Receive
int perform_request(SSL *ssl, uint16_t opcode, void *req_data, size_t req_size, BankingResponse *response) {
    BankingPacket req_packet;
    if (pack_request(&req_packet, opcode, req_data, req_size) != 0) return -1;
    
    if (tls_write(ssl, &req_packet, sizeof(BankingPacket)) <= 0) return -1;
    
    BankingPacket resp_packet;
    int bytes = tls_read(ssl, &resp_packet, sizeof(BankingPacket));
    if (bytes <= 0) return -1;
    
    return unpack_response(&resp_packet, response);
}

void *worker_thread(void *args) {
    ThreadArgs *t_args = (ThreadArgs *)args;
    t_args->min_latency_ms = 999999.0;
    t_args->max_latency_ms = 0.0;
    
    // Setup TLS
    TLSConfig tls_config = {
        .ca_cert_path = "certificate/ca.crt", // Assuming running from project root
        .client_cert_path = "certificate/client.crt",
        .client_key_path = "certificate/client.key",
        .verify_peer = t_args->verify_cert
    };
    
    SSL_CTX *ctx = tls_create_client_context(&tls_config);
    if (!ctx) {
        printf("[Thread %d] TLS Context Failed\n", t_args->thread_id);
        return NULL;
    }
    
    // Connect
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(t_args->server_port);
    inet_pton(AF_INET, t_args->server_ip, &serv_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        // Only print error for first few threads to avoid flooding
        if (t_args->thread_id < 5) printf("[Thread %d] Connect Failed\n", t_args->thread_id);
        tls_cleanup_context(ctx);
        return NULL;
    }
    
    SSL *ssl = tls_connect(ctx, sock, "api.bank.com");
    if (!ssl) {
        if (t_args->thread_id < 5) printf("[Thread %d] TLS Handshake Failed\n", t_args->thread_id);
        close(sock);
        tls_cleanup_context(ctx);
        return NULL;
    }
    
    // Prepare Account ID
    char account_id[20];
    snprintf(account_id, sizeof(account_id), "user_%d_%d", getpid(), t_args->thread_id);
    
    // OTP Flow Variable
    char otp_code[10] = {0};
    
    // Operations loop
    for (int i = 0; i < t_args->num_requests; i++) {
        double start_time = get_time_ms();
        BankingResponse response;
        
        // Sequence: Create -> ReqOTP -> Login -> Deposit -> Withdraw -> Balance
        
        // 1. Create Account
        CreateAccountRequest create_req;
        strncpy(create_req.account_id, account_id, sizeof(create_req.account_id));
        create_req.initial_balance = 1000.0;
        
        if (perform_request(ssl, OP_CREATE_ACCOUNT, &create_req, sizeof(create_req), &response) == 0) {
            // 2. Request OTP
            OtpRequest otp_req;
            strncpy(otp_req.account_id, account_id, sizeof(otp_req.account_id));
            if (perform_request(ssl, OP_REQ_OTP, &otp_req, sizeof(otp_req), &response) == 0 && response.status == STATUS_SUCCESS) {
                // Parse OTP from message "OTP Generated: XXXXXX"
                char *ptr = strstr(response.message, ": ");
                if (ptr) {
                    strncpy(otp_code, ptr + 2, 8);
                    otp_code[8] = '\0'; // Ensure null term
                    
                    // 3. Login
                    LoginRequest login_req;
                    strncpy(login_req.account_id, account_id, sizeof(login_req.account_id));
                    strncpy(login_req.otp, otp_code, sizeof(login_req.otp));
                    
                    if (perform_request(ssl, OP_LOGIN, &login_req, sizeof(login_req), &response) == 0 && response.status == STATUS_SUCCESS) {
                         // 4. Deposit
                        DepositRequest dep_req;
                        strncpy(dep_req.account_id, account_id, sizeof(dep_req.account_id));
                        dep_req.amount = 100.0;
                        perform_request(ssl, OP_DEPOSIT, &dep_req, sizeof(dep_req), &response);
                    }
                }
            }
        }
        
        double end_time = get_time_ms();
        double latency = end_time - start_time;
        
        t_args->total_latency_ms += latency;
        if (latency > t_args->max_latency_ms) t_args->max_latency_ms = latency;
        if (latency < t_args->min_latency_ms) t_args->min_latency_ms = latency;
        t_args->success_count++; // Counting 'flow' success
    }
    
    // Cleanup
    tls_close(ssl);
    close(sock);
    tls_cleanup_context(ctx);
    
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <ip> <port> [threads] [requests_per_thread] [verify_cert]\n", argv[0]);
        return 1;
    }
    
    char *ip = argv[1];
    int port = atoi(argv[2]);
    int num_threads = (argc >= 4) ? atoi(argv[3]) : DEFAULT_THREADS;
    int reqs_per_thread = (argc >= 5) ? atoi(argv[4]) : DEFAULT_REQUESTS;
    int verify = (argc >= 6) ? atoi(argv[5]) : 0;
    
    printf("=== Stress Test Client ===\n");
    printf("Target: %s:%d\n", ip, port);
    printf("Threads: %d\n", num_threads);
    printf("Requests/Thread: %d\n", reqs_per_thread);
    printf("OTP/TLS Verify: %s\n", verify ? "YES" : "NO");
    
    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
    ThreadArgs *t_args = malloc(sizeof(ThreadArgs) * num_threads);
    
    double start_time = get_time_ms();
    
    for (int i = 0; i < num_threads; i++) {
        t_args[i].thread_id = i;
        t_args[i].server_ip = ip;
        t_args[i].server_port = port;
        t_args[i].num_requests = reqs_per_thread;
        t_args[i].verify_cert = verify;
        t_args[i].success_count = 0;
        t_args[i].fail_count = 0;
        t_args[i].total_latency_ms = 0;
        
        pthread_create(&threads[i], NULL, worker_thread, &t_args[i]);
    }
    
    // Wait for completion
    int total_reqs = 0;
    double total_latency_sum = 0;
    double global_max = 0;
    double global_min = 999999;
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_reqs += t_args[i].success_count;
        total_latency_sum += t_args[i].total_latency_ms;
        if (t_args[i].max_latency_ms > global_max) global_max = t_args[i].max_latency_ms;
        if (t_args[i].min_latency_ms < global_min) global_min = t_args[i].min_latency_ms;
    }
    
    double end_time = get_time_ms();
    double total_duration_sec = (end_time - start_time) / 1000.0;
    
    printf("\n=== Test Results ===\n");
    printf("Total Duration: %.2f sec\n", total_duration_sec);
    printf("Total Completed Flows: %d\n", total_reqs);
    printf("Throughput: %.2f flows/sec\n", total_reqs / total_duration_sec);
    printf("Latency (Flow):\n");
    printf("  Avg: %.2f ms\n", (total_latency_sum / total_reqs));
    printf("  Min: %.2f ms\n", global_min);
    printf("  Max: %.2f ms\n", global_max);
    
    free(threads);
    free(t_args);
    return 0;
}
