/*
 * secure_client.c
 * Compile: gcc secure_client.c -o secure_client -lssl -lcrypto
 * Usage: ./secure_client <ip> <port> <verify_server (0/1)>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFFER_SIZE 1024

void handle_error(const char *msg) {
    perror(msg);
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
}

SSL_CTX *create_context() {
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) handle_error("Unable to create SSL context");
    return ctx;
}

void configure_context(SSL_CTX *ctx, int verify_server) {
    // 1. Load CA Certificate to verify Server's identity
    // If verify_server is 1, this is crucial.
    if (SSL_CTX_load_verify_locations(ctx, "certificate/ca.crt", NULL) <= 0) {
        handle_error("Failed to load CA certificate");
    }

    // 2. Load Client's Certificate & Key (For mTLS scenarios)
    // We load this regardless, so the client is ready if the server requests it.
    if (SSL_CTX_use_certificate_file(ctx, "certificate/client.crt", SSL_FILETYPE_PEM) > 0 &&
        SSL_CTX_use_PrivateKey_file(ctx, "certificate/client.key", SSL_FILETYPE_PEM) > 0) {
        // Successfully loaded client certs
    } else {
        printf("[Info] Client certificate not loaded (optional).\n");
    }

    // 3. Set Verification Mode
    if (verify_server) {
        printf("[Info] Verification: ON. Validating Server Certificate.\n");
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    } else {
        printf("[Info] Verification: OFF. Skipping Server Validation.\n");
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Usage: %s <ip> <port> <verify_server (0=No, 1=Yes)>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int verify_server = atoi(argv[3]);

    SSL_CTX *ctx = create_context();
    configure_context(ctx, verify_server);

    // Standard TCP Connection
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        handle_error("TCP Connection failed");
    }

    // TLS Setup
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);

    // Set Hostname for SNI (Server Name Indication) - Good practice
    SSL_set_tlsext_host_name(ssl, "api.bank.com");

    // Perform TLS Handshake
    if (SSL_connect(ssl) <= 0) {
        printf("[Error] TLS Handshake failed.\n");
        ERR_print_errors_fp(stderr);
    } else {
        printf("[Success] Connected with %s encryption\n", SSL_get_cipher(ssl));

        // Explicitly check verification result if required
        if (verify_server) {
            long verify_result = SSL_get_verify_result(ssl);
            if (verify_result != X509_V_OK) {
                printf("[Security Error] Certificate Verification Failed: %ld\n", verify_result);
                // In a real app, you might want to close connection here
            } else {
                printf("[Security] Server Certificate Verified OK.\n");
            }
        }

        char *msg = "Hello Secure Server!";
        SSL_write(ssl, msg, strlen(msg));

        char buf[BUFFER_SIZE] = {0};
        SSL_read(ssl, buf, sizeof(buf));
        printf("Server replied: %s\n", buf);
    }

    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    return 0;
}
