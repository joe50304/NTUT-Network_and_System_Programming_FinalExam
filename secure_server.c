/*
 * secure_server.c
 * Compile: gcc secure_server.c -o secure_server -lssl -lcrypto
 * Usage: ./secure_server <port> <verify_client_flag (0 or 1)>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFFER_SIZE 1024

/* Error handling helper */
void handle_error(const char *msg) {
    perror(msg);
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
}

/* Initialize OpenSSL Context */
SSL_CTX *create_context() {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = TLS_server_method();
    ctx = SSL_CTX_new(method);
    if (!ctx) handle_error("Unable to create SSL context");

    return ctx;
}

/* Load certificates and configure verification settings */
void configure_context(SSL_CTX *ctx, int verify_client) {
    // 1. Load Server's Certificate (Public Key)
    if (SSL_CTX_use_certificate_file(ctx, "certificate/server_wildcard.crt", SSL_FILETYPE_PEM) <= 0) {
        handle_error("Failed to load server certificate");
    }

    // 2. Load Server's Private Key
    if (SSL_CTX_use_PrivateKey_file(ctx, "certificate/server_wildcard.key", SSL_FILETYPE_PEM) <= 0) {
        handle_error("Failed to load server private key");
    }

    // 3. Load the CA Certificate to verify the Client's certificate (Trust Store)
    if (SSL_CTX_load_verify_locations(ctx, "certificate/ca.crt", NULL) <= 0) {
        handle_error("Failed to load CA certificate");
    }

    // 4. Configure Client Verification Mode
    if (verify_client) {
        printf("[Info] Mutual TLS Enabled: Server will verify Client certificate.\n");
        // SSL_VERIFY_PEER: Request a cert from the client.
        // SSL_VERIFY_FAIL_IF_NO_PEER_CERT: Reject connection if client sends no cert.
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    } else {
        printf("[Info] Standard TLS: Server will NOT verify Client certificate.\n");
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <port> <verify_client (0=No, 1=Yes)>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int verify_client = atoi(argv[2]);

    int server_fd, client_fd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    SSL_CTX *ctx;

    // Initialize SSL
    ctx = create_context();
    configure_context(ctx, verify_client);

    // Create TCP Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) handle_error("Socket creation failed");

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    // Bind and Listen
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
        handle_error("Bind failed");
    
    if (listen(server_fd, 1) < 0) 
        handle_error("Listen failed");

    printf("Server listening on port %d...\n", port);

    // Accept loop (Handle one client for demonstration)
    client_fd = accept(server_fd, (struct sockaddr*)&addr, &addr_len);
    if (client_fd < 0) handle_error("Accept failed");

    // Create SSL structure for this connection
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, client_fd);

    // Perform TLS Handshake
    if (SSL_accept(ssl) <= 0) {
        printf("[Error] TLS Handshake failed.\n");
        ERR_print_errors_fp(stderr);
    } else {
        printf("[Success] TLS connection established!\n");
        printf("Cipher: %s\n", SSL_get_cipher(ssl));

        // If configured, check Client Certificate result
        if (verify_client) {
            X509 *cert = SSL_get_peer_certificate(ssl);
            if (cert) {
                printf("[Security] Client certificate verified.\n");
                X509_free(cert);
            } else {
                printf("[Security] Warning: No client certificate received.\n");
            }
        }

        // Read Data
        char buf[BUFFER_SIZE] = {0};
        int bytes = SSL_read(ssl, buf, sizeof(buf) - 1);
        if (bytes > 0) {
            printf("Client sent: %s\n", buf);
            const char *reply = "Message received securely.";
            SSL_write(ssl, reply, strlen(reply));
        }
    }

    // Cleanup
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_fd);
    close(server_fd);
    SSL_CTX_free(ctx);

    return 0;
}
