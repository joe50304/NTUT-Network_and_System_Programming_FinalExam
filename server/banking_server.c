/*
 * banking_server.c
 * Integrated Banking Server with TLS, Multi-Process Architecture, and IPC
 * 
 * Architecture:
 * - Master Process: Listens for connections, forks workers
 * - Worker Processes: Handle client requests with TLS
 * - Shared Memory: AccountDB with mutex locking
 * 
 * Compile: gcc banking_server.c ../common/*.c -o banking_server -lssl -lcrypto -lpthread
 * Usage: ./banking_server <port> [verify_client]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <errno.h>

#include "../common/include/protocol.h"
#include "../common/include/account.h"
#include "../common/include/ipc.h"
#include "../common/include/tls_wrapper.h"
#include "../common/include/otp_ipc.h"

#define MAX_WORKERS 5
#define DEFAULT_PORT 8888
#define BACKLOG 10

// Global variables
static volatile sig_atomic_t keep_running = 1;
static pid_t worker_pids[MAX_WORKERS];
static int server_fd = -1;
static SSL_CTX *ssl_ctx = NULL;

// Signal handler for graceful shutdown
void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        if (keep_running) {  // Only print once
            printf("\n[Master] Received signal %d, initiating graceful shutdown...\n", signum);
            keep_running = 0;

            // Close listening socket to unblock accept()
            if (server_fd >= 0) {
                close(server_fd);
                server_fd = -1;
            }
        }
    }
}

// SIGCHLD handler to reap zombie processes
void sigchld_handler(int signum) {
    (void)signum;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// 內部 helper: 送出 Struct 給 OTP Server 並接收回應
int call_otp_service(int opcode, const char *account, const char *otp_in, char *otp_out) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(OTP_PORT);
    inet_pton(AF_INET, OTP_IP, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Cannot connect to OTP Server");
        close(sock);
        return 0;
    }

    // 1. 準備請求包
    OtpIpcRequest req;
    memset(&req, 0, sizeof(req));
    req.op_code = opcode;
    strncpy(req.account, account, sizeof(req.account) - 1);
    if (otp_in) strncpy(req.otp_code, otp_in, sizeof(req.otp_code) - 1);

    // 2. 傳送
    write(sock, &req, sizeof(req));

    // 3. 接收回應
    OtpIpcResponse res;
    int bytes = read(sock, &res, sizeof(res));
    close(sock);

    if (bytes > 0 && res.status == 1) {
        if (otp_out) strncpy(otp_out, res.otp_code, 8); // 如果是 Gen，把 OTP 帶出來
        return 1; // 成功
    }
    return 0; // 失敗
}

// 替換原本的 request_otp_generation
int request_otp_generation(const char *account, char *out_otp) {
    // 呼叫我們的自定義協定
    return call_otp_service(OTP_OP_GENERATE, account, NULL, out_otp);
}

// 替換原本的 verify_otp_remote
int verify_otp_remote(const char *account, const char *otp) {
    // 呼叫我們的自定義協定
    return call_otp_service(OTP_OP_VERIFY, account, otp, NULL);
}

// Process client request
void process_request(SSL *ssl, AccountDB *db, const BankingPacket *req_packet) {
    BankingResponse response;
    memset(&response, 0, sizeof(response));
    
    uint16_t opcode = ntohs(req_packet->header.op_code);
    
    switch (opcode) {
        case OP_CREATE_ACCOUNT: {
            CreateAccountRequest req;
            if (unpack_request(req_packet, &req, sizeof(req)) == 0) {
                int result = account_create(db, req.account_id, req.initial_balance);
                response.status = result;
                response.balance = req.initial_balance;
                
                if (result == 0) {
                    snprintf(response.message, sizeof(response.message),
                            "Account %s created successfully", req.account_id);
                } else if (result == -4) {
                    snprintf(response.message, sizeof(response.message),
                            "Account %s already exists", req.account_id);
                } else if (result == -5) {
                    snprintf(response.message, sizeof(response.message),
                            "Database full, cannot create account");
                } else {
                    snprintf(response.message, sizeof(response.message),
                            "Failed to create account");
                }
            } else {
                response.status = STATUS_ERROR;
                snprintf(response.message, sizeof(response.message), "Invalid request format");
            }
            break;
        }
        
        case OP_DEPOSIT: {
            DepositRequest req;
            if (unpack_request(req_packet, &req, sizeof(req)) == 0) {
                double new_balance;
                int result = account_deposit(db, req.account_id, req.amount, &new_balance);
                response.status = result;
                response.balance = new_balance;
                
                if (result == 0) {
                    snprintf(response.message, sizeof(response.message),
                            "Deposited %.2f to account %s", req.amount, req.account_id);
                } else if (result == -2) {
                    snprintf(response.message, sizeof(response.message),
                            "Account %s not found", req.account_id);
                } else {
                    snprintf(response.message, sizeof(response.message),
                            "Deposit failed");
                }
            } else {
                response.status = STATUS_ERROR;
                snprintf(response.message, sizeof(response.message), "Invalid request format");
            }
            break;
        }
        
        case OP_WITHDRAW: {
            WithdrawRequest req;
            if (unpack_request(req_packet, &req, sizeof(req)) == 0) {
                double new_balance;
                int result = account_withdraw(db, req.account_id, req.amount, &new_balance);
                response.status = result;
                response.balance = new_balance;
                
                if (result == 0) {
                    snprintf(response.message, sizeof(response.message),
                            "Withdrew %.2f from account %s", req.amount, req.account_id);
                } else if (result == -2) {
                    snprintf(response.message, sizeof(response.message),
                            "Account %s not found", req.account_id);
                } else if (result == -3) {
                    snprintf(response.message, sizeof(response.message),
                            "Insufficient funds");
                } else {
                    snprintf(response.message, sizeof(response.message),
                            "Withdrawal failed");
                }
            } else {
                response.status = STATUS_ERROR;
                snprintf(response.message, sizeof(response.message), "Invalid request format");
            }
            break;
        }
        
        case OP_REQ_OTP: {
            OtpRequest req;
            if (unpack_request(req_packet, &req, sizeof(req)) == 0) {
                char otp_code[10] = {0};
                if (request_otp_generation(req.account_id, otp_code)) {
                     response.status = STATUS_SUCCESS;
                     // In a real system, OTP is sent via SMS. Here we return it for testing convenience
                     snprintf(response.message, sizeof(response.message), "OTP Generated: %s", otp_code);
                } else {
                     response.status = STATUS_ERROR;
                     snprintf(response.message, sizeof(response.message), "OTP Generation Failed");
                }
            } else {
                response.status = STATUS_ERROR;
                snprintf(response.message, sizeof(response.message), "Invalid request format");
            }
            break;
        }

        case OP_LOGIN: {
            LoginRequest req;
             if (unpack_request(req_packet, &req, sizeof(req)) == 0) {
                if (verify_otp_remote(req.account_id, req.otp)) {
                    response.status = STATUS_SUCCESS;
                    snprintf(response.message, sizeof(response.message), "Login Successful");
                } else {
                    response.status = STATUS_ERROR;
                    snprintf(response.message, sizeof(response.message), "Invalid OTP");
                }
            } else {
                response.status = STATUS_ERROR;
                 snprintf(response.message, sizeof(response.message), "Invalid request format");
            }
            break;
        }

        case OP_BALANCE: {
            BalanceRequest req;
            if (unpack_request(req_packet, &req, sizeof(req)) == 0) {
                double balance;
                int result = account_get_balance(db, req.account_id, &balance);
                response.status = result;
                response.balance = balance;
                
                if (result == 0) {
                    snprintf(response.message, sizeof(response.message),
                            "Account %s balance: %.2f", req.account_id, balance);
                } else if (result == -2) {
                    snprintf(response.message, sizeof(response.message),
                            "Account %s not found", req.account_id);
                } else {
                    snprintf(response.message, sizeof(response.message),
                            "Query failed");
                }
            } else {
                response.status = STATUS_ERROR;
                snprintf(response.message, sizeof(response.message), "Invalid request format");
            }
            break;
        }
        
        default:
            response.status = STATUS_ERROR;
            snprintf(response.message, sizeof(response.message), "Unknown operation");
            break;
    }
    
    // Send response
    BankingPacket resp_packet;
    pack_response(&resp_packet, &response);
    tls_write(ssl, &resp_packet, sizeof(BankingPacket));
}

// Worker process main loop
void worker_main(int worker_id, AccountDB *db) {
    printf("[Worker %d] Started (PID: %d)\n", worker_id, getpid());
    
    // Worker signal handler for graceful shutdown
    void worker_signal_handler(int signum) {
        if (signum == SIGTERM) {
            printf("[Worker %d] Received shutdown signal, exiting...\n", worker_id);
            exit(0);
        }
    }
    
    signal(SIGTERM, worker_signal_handler);
    
    while (1) {
        // Accept connection (blocking, will be interrupted by master shutdown)
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR || errno == EBADF) {
                // Server socket closed, time to exit
                break;
            }
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("[Worker %d] Accepted connection from %s:%d\n",
               worker_id, client_ip, ntohs(client_addr.sin_port));
        
        // TLS Handshake
        SSL *ssl = tls_accept_connection(ssl_ctx, client_fd);
        if (!ssl) {
            printf("[Worker %d] TLS handshake failed\n", worker_id);
            close(client_fd);
            continue;
        }
        
        printf("[Worker %d] TLS connection established (Cipher: %s)\n",
               worker_id, SSL_get_cipher(ssl));
        
        // Handle client requests
        BankingPacket packet;
        while (1) {
            int bytes = tls_read(ssl, &packet, sizeof(BankingPacket));
            
            if (bytes <= 0) {
                // Connection closed or error
                break;
            }
            
            if (bytes != sizeof(BankingPacket)) {
                printf("[Worker %d] Invalid packet size: %d\n", worker_id, bytes);
                break;
            }
            
            // Verify checksum
            if (verify_packet_checksum(&packet) != 0) {
                printf("[Worker %d] Checksum verification failed\n", worker_id);
                BankingResponse error_resp;
                memset(&error_resp, 0, sizeof(error_resp));
                error_resp.status = STATUS_ERROR;
                snprintf(error_resp.message, sizeof(error_resp.message),
                        "Checksum verification failed");
                
                BankingPacket error_packet;
                pack_response(&error_packet, &error_resp);
                tls_write(ssl, &error_packet, sizeof(BankingPacket));
                continue;
            }
            
            // Process request
            process_request(ssl, db, &packet);
        }
        
        printf("[Worker %d] Client disconnected\n", worker_id);
        tls_close(ssl);
        close(client_fd);
    }
    
    printf("[Worker %d] Shutting down\n", worker_id);
    exit(0);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <port> [verify_client (0=No, 1=Yes)]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int port = atoi(argv[1]);
    int verify_client = (argc >= 3) ? atoi(argv[2]) : 0;
    
    printf("=== Banking Server Starting ===\n");
    printf("Port: %d\n", port);
    printf("Workers: %d\n", MAX_WORKERS);
    printf("Client Verification: %s\n", verify_client ? "YES (mTLS)" : "NO");
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGCHLD, sigchld_handler);
    signal(SIGPIPE, SIG_IGN);
    
    // Initialize TLS
    TLSConfig tls_config = {
        .ca_cert_path = DEFAULT_CA_CERT,
        .server_cert_path = DEFAULT_SERVER_CERT,
        .server_key_path = DEFAULT_SERVER_KEY,
        .verify_peer = verify_client
    };
    
    ssl_ctx = tls_create_server_context(&tls_config);
    if (!ssl_ctx) {
        fprintf(stderr, "Failed to create TLS context\n");
        exit(EXIT_FAILURE);
    }
    printf("[Master] TLS context initialized\n");
    
    // Initialize Shared Memory (IPC)
    IPCContext ipc_ctx;
    if (ipc_init_server(&ipc_ctx) != 0) {
        fprintf(stderr, "Failed to create shared memory\n");
        tls_cleanup_context(ssl_ctx);
        exit(EXIT_FAILURE);
    }
    AccountDB *db = ipc_get_db(&ipc_ctx);
    printf("[Master] Shared memory initialized (Size: %lu bytes)\n", sizeof(AccountDB));
    
    // Create TCP Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        ipc_cleanup(&ipc_ctx, 1);
        tls_cleanup_context(ssl_ctx);
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        ipc_cleanup(&ipc_ctx, 1);
        tls_cleanup_context(ssl_ctx);
        exit(EXIT_FAILURE);
    }
    
    // Listen
    if (listen(server_fd, BACKLOG) < 0) {
        perror("Listen failed");
        close(server_fd);
        ipc_cleanup(&ipc_ctx, 1);
        tls_cleanup_context(ssl_ctx);
        exit(EXIT_FAILURE);
    }
    
    printf("[Master] Listening on port %d\n", port);
    
    // Fork worker processes
    for (int i = 0; i < MAX_WORKERS; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("Fork failed");
            continue;
        } else if (pid == 0) {
            // Child process (Worker)
            worker_main(i, db);
            // Should never reach here
            exit(0);
        } else {
            // Parent process (Master)
            worker_pids[i] = pid;
        }
    }
    
    printf("[Master] All workers spawned, ready to accept connections\n");
    printf("[Master] Press Ctrl+C to shutdown gracefully\n");
    
    // Master waits for shutdown signal
    while (keep_running) {
        pause();  // Wait for signal
    }
    
    // Graceful shutdown
    printf("\n[Master] Shutting down workers...\n");
    for (int i = 0; i < MAX_WORKERS; i++) {
        if (worker_pids[i] > 0) {
            kill(worker_pids[i], SIGTERM);
        }
    }
    
    // Wait for all workers to terminate
    for (int i = 0; i < MAX_WORKERS; i++) {
        if (worker_pids[i] > 0) {
            waitpid(worker_pids[i], NULL, 0);
            printf("[Master] Worker %d terminated\n", i);
        }
    }
    
    // Cleanup
    if (server_fd >= 0) {
        close(server_fd);
    }
    ipc_cleanup(&ipc_ctx, 1);
    tls_cleanup_context(ssl_ctx);
    
    printf("[Master] Shutdown complete\n");
    return 0;
}
