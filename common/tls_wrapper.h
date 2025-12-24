/*
 * tls_wrapper.h
 * TLS/SSL Wrapper for Secure Communication
 */

#ifndef TLS_WRAPPER_H
#define TLS_WRAPPER_H

#include <openssl/ssl.h>
#include <openssl/err.h>

// TLS Context Configuration
typedef struct {
    const char *ca_cert_path;
    const char *server_cert_path;
    const char *server_key_path;
    const char *client_cert_path;
    const char *client_key_path;
    int verify_peer;  // 1 = verify, 0 = no verify
} TLSConfig;

// Default configuration
#define DEFAULT_CA_CERT       "certificate/ca.crt"
#define DEFAULT_SERVER_CERT   "certificate/server_wildcard.crt"
#define DEFAULT_SERVER_KEY    "certificate/server_wildcard.key"
#define DEFAULT_CLIENT_CERT   "certificate/client.crt"
#define DEFAULT_CLIENT_KEY    "certificate/client.key"

// TLS Functions
SSL_CTX *tls_create_server_context(const TLSConfig *config);
SSL_CTX *tls_create_client_context(const TLSConfig *config);
SSL *tls_accept_connection(SSL_CTX *ctx, int client_fd);
SSL *tls_connect(SSL_CTX *ctx, int sock_fd, const char *hostname);
int tls_read(SSL *ssl, void *buf, int len);
int tls_write(SSL *ssl, const void *buf, int len);
void tls_close(SSL *ssl);
void tls_cleanup_context(SSL_CTX *ctx);
void tls_print_error(const char *msg);

#endif // TLS_WRAPPER_H
