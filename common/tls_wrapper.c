/*
 * tls_wrapper.c
 * TLS/SSL Wrapper Implementation
 */

#include "tls_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Print OpenSSL errors
void tls_print_error(const char *msg) {
    fprintf(stderr, "[TLS Error] %s\n", msg);
    ERR_print_errors_fp(stderr);
}

// Create Server SSL Context
SSL_CTX *tls_create_server_context(const TLSConfig *config) {
    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    
    if (!ctx) {
        tls_print_error("Unable to create SSL context");
        return NULL;
    }
    
    // Load server certificate
    if (SSL_CTX_use_certificate_file(ctx, config->server_cert_path, SSL_FILETYPE_PEM) <= 0) {
        tls_print_error("Failed to load server certificate");
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    // Load server private key
    if (SSL_CTX_use_PrivateKey_file(ctx, config->server_key_path, SSL_FILETYPE_PEM) <= 0) {
        tls_print_error("Failed to load server private key");
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    // Load CA certificate for client verification
    if (SSL_CTX_load_verify_locations(ctx, config->ca_cert_path, NULL) <= 0) {
        tls_print_error("Failed to load CA certificate");
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    // Set verification mode
    if (config->verify_peer) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    } else {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    }
    
    return ctx;
}

// Create Client SSL Context
SSL_CTX *tls_create_client_context(const TLSConfig *config) {
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    
    if (!ctx) {
        tls_print_error("Unable to create SSL context");
        return NULL;
    }
    
    // Load CA certificate to verify server
    if (SSL_CTX_load_verify_locations(ctx, config->ca_cert_path, NULL) <= 0) {
        tls_print_error("Failed to load CA certificate");
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    // Load client certificate and key (optional, for mTLS)
    if (config->client_cert_path && config->client_key_path) {
        if (SSL_CTX_use_certificate_file(ctx, config->client_cert_path, SSL_FILETYPE_PEM) > 0 &&
            SSL_CTX_use_PrivateKey_file(ctx, config->client_key_path, SSL_FILETYPE_PEM) > 0) {
            // Client cert loaded successfully
        }
    }
    
    // Set verification mode
    if (config->verify_peer) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    } else {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    }
    
    return ctx;
}

// Accept TLS connection (Server side)
SSL *tls_accept_connection(SSL_CTX *ctx, int client_fd) {
    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        tls_print_error("Failed to create SSL structure");
        return NULL;
    }
    
    SSL_set_fd(ssl, client_fd);
    
    if (SSL_accept(ssl) <= 0) {
        tls_print_error("TLS handshake failed");
        SSL_free(ssl);
        return NULL;
    }
    
    return ssl;
}

// Connect with TLS (Client side)
SSL *tls_connect(SSL_CTX *ctx, int sock_fd, const char *hostname) {
    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        tls_print_error("Failed to create SSL structure");
        return NULL;
    }
    
    SSL_set_fd(ssl, sock_fd);
    
    // Set hostname for SNI
    if (hostname) {
        SSL_set_tlsext_host_name(ssl, hostname);
    }
    
    if (SSL_connect(ssl) <= 0) {
        tls_print_error("TLS connection failed");
        SSL_free(ssl);
        return NULL;
    }
    
    return ssl;
}

// Read from TLS connection
int tls_read(SSL *ssl, void *buf, int len) {
    return SSL_read(ssl, buf, len);
}

// Write to TLS connection
int tls_write(SSL *ssl, const void *buf, int len) {
    return SSL_write(ssl, buf, len);
}

// Close TLS connection
void tls_close(SSL *ssl) {
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
}

// Cleanup SSL context
void tls_cleanup_context(SSL_CTX *ctx) {
    if (ctx) {
        SSL_CTX_free(ctx);
    }
}
