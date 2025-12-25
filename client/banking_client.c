/*
 * banking_client.c
 * Banking Client with TLS Support
 * 
 * Compile: gcc banking_client.c ../common/*.c -o banking_client -lssl -lcrypto -lpthread
 * Usage: ./banking_client <ip> <port> [verify_server]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "protocol.h"
#include "tls_wrapper.h"

#define BUFFER_SIZE 1024

// Send request and receive response
int send_request(SSL *ssl, uint16_t opcode, const void *req_data, size_t req_size, BankingResponse *response) {
    // Pack request
    BankingPacket req_packet;
    if (pack_request(&req_packet, opcode, req_data, req_size) != 0) {
        printf("Failed to pack request\n");
        return -1;
    }
    
    // Send request
    if (tls_write(ssl, &req_packet, sizeof(BankingPacket)) <= 0) {
        printf("Failed to send request\n");
        return -1;
    }
    
    // Receive response
    BankingPacket resp_packet;
    int bytes = tls_read(ssl, &resp_packet, sizeof(BankingPacket));
    if (bytes <= 0) {
        printf("Failed to receive response\n");
        return -1;
    }
    
    // Unpack response
    if (unpack_response(&resp_packet, response) != 0) {
        printf("Failed to unpack response\n");
        return -1;
    }
    
    return 0;
}

// Menu operations
void menu_create_account(SSL *ssl) {
    CreateAccountRequest req;
    printf("\n=== Create Account ===\n");
    printf("Enter Account ID: ");
    scanf("%s", req.account_id);
    printf("Enter Initial Balance: ");
    scanf("%lf", &req.initial_balance);
    
    BankingResponse response;
    if (send_request(ssl, OP_CREATE_ACCOUNT, &req, sizeof(req), &response) == 0) {
        printf("\nStatus: %d\n", response.status);
        printf("Message: %s\n", response.message);
        if (response.status == 0) {
            printf("Balance: %.2f\n", response.balance);
        }
    }
}

void menu_deposit(SSL *ssl) {
    DepositRequest req;
    printf("\n=== Deposit ===\n");
    printf("Enter Account ID: ");
    scanf("%s", req.account_id);
    printf("Enter Amount: ");
    scanf("%lf", &req.amount);
    
    BankingResponse response;
    if (send_request(ssl, OP_DEPOSIT, &req, sizeof(req), &response) == 0) {
        printf("\nStatus: %d\n", response.status);
        printf("Message: %s\n", response.message);
        if (response.status == 0) {
            printf("New Balance: %.2f\n", response.balance);
        }
    }
}

void menu_withdraw(SSL *ssl) {
    WithdrawRequest req;
    printf("\n=== Withdraw ===\n");
    printf("Enter Account ID: ");
    scanf("%s", req.account_id);
    printf("Enter Amount: ");
    scanf("%lf", &req.amount);
    
    BankingResponse response;
    if (send_request(ssl, OP_WITHDRAW, &req, sizeof(req), &response) == 0) {
        printf("\nStatus: %d\n", response.status);
        printf("Message: %s\n", response.message);
        if (response.status == 0) {
            printf("New Balance: %.2f\n", response.balance);
        }
    }
}

void menu_check_balance(SSL *ssl) {
    BalanceRequest req;
    printf("\n=== Check Balance ===\n");
    printf("Enter Account ID: ");
    scanf("%s", req.account_id);
    
    BankingResponse response;
    if (send_request(ssl, OP_BALANCE, &req, sizeof(req), &response) == 0) {
        printf("\nStatus: %d\n", response.status);
        printf("Message: %s\n", response.message);
        if (response.status == 0) {
            printf("Balance: %.2f\n", response.balance);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <ip> <port> [verify_server (0=No, 1=Yes)]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char *ip = argv[1];
    int port = atoi(argv[2]);
    int verify_server = (argc >= 4) ? atoi(argv[3]) : 0;
    
    printf("=== Banking Client ===\n");
    printf("Connecting to %s:%d\n", ip, port);
    printf("Server Verification: %s\n", verify_server ? "YES" : "NO");
    
    // Initialize TLS context
    TLSConfig tls_config = {
        .ca_cert_path = DEFAULT_CA_CERT,
        .client_cert_path = DEFAULT_CLIENT_CERT,
        .client_key_path = DEFAULT_CLIENT_KEY,
        .verify_peer = verify_server
    };
    
    SSL_CTX *ctx = tls_create_client_context(&tls_config);
    if (!ctx) {
        fprintf(stderr, "Failed to create TLS context\n");
        exit(EXIT_FAILURE);
    }
    
    // Create TCP connection
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        tls_cleanup_context(ctx);
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        tls_cleanup_context(ctx);
        exit(EXIT_FAILURE);
    }
    
    printf("TCP connection established\n");
    
    // TLS handshake
    SSL *ssl = tls_connect(ctx, sock, "api.bank.com");
    if (!ssl) {
        fprintf(stderr, "TLS handshake failed\n");
        close(sock);
        tls_cleanup_context(ctx);
        exit(EXIT_FAILURE);
    }
    
    printf("TLS connection established (Cipher: %s)\n", SSL_get_cipher(ssl));
    
    if (verify_server) {
        long verify_result = SSL_get_verify_result(ssl);
        if (verify_result == X509_V_OK) {
            printf("Server certificate verified OK\n");
        } else {
            printf("WARNING: Certificate verification failed: %ld\n", verify_result);
        }
    }
    
    // Interactive menu
    printf("\n=== Connected to Banking Server ===\n");
    
    int choice;
    while (1) {
        printf("\n--- Banking Menu ---\n");
        printf("1. Create Account\n");
        printf("2. Deposit\n");
        printf("3. Withdraw\n");
        printf("4. Check Balance\n");
        printf("5. Exit\n");
        printf("Enter choice: ");
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input\n");
            while (getchar() != '\n');
            continue;
        }
        
        switch (choice) {
            case 1:
                menu_create_account(ssl);
                break;
            case 2:
                menu_deposit(ssl);
                break;
            case 3:
                menu_withdraw(ssl);
                break;
            case 4:
                menu_check_balance(ssl);
                break;
            case 5:
                printf("Goodbye!\n");
                goto cleanup;
            default:
                printf("Invalid choice\n");
                break;
        }
    }
    
cleanup:
    tls_close(ssl);
    close(sock);
    tls_cleanup_context(ctx);
    
    return 0;
}
