/*
 * protocol.h
 * Custom Banking Protocol Definition
 * Format: [Packet Length (4 bytes)] + [OpCode (2 bytes)] + [Checksum (2 bytes)] + [Data Content]
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// Protocol Constants
#define MAX_DATA_SIZE 1024
#define PROTOCOL_HEADER_SIZE 8  // 4 (length) + 2 (opcode) + 2 (checksum)

// Operation Codes
#define OP_CREATE_ACCOUNT  0x0001
#define OP_DEPOSIT         0x0002
#define OP_WITHDRAW        0x0003
#define OP_BALANCE         0x0004
#define OP_RESPONSE        0x00FF

// Response Status Codes
#define STATUS_SUCCESS            0
#define STATUS_ERROR             -1
#define STATUS_ACCOUNT_NOT_FOUND -2
#define STATUS_INSUFFICIENT_FUNDS -3
#define STATUS_ACCOUNT_EXISTS    -4
#define STATUS_DB_FULL           -5
#define STATUS_INVALID_AMOUNT    -6

// Banking Packet Structure
typedef struct {
    uint32_t packet_length;      // Total packet size (including header)
    uint16_t opcode;             // Operation code
    uint16_t checksum;           // CRC16 checksum for data integrity
    char data[MAX_DATA_SIZE];    // Variable length payload
} __attribute__((packed)) BankingPacket;

// Request/Response Payload Structures
typedef struct {
    char account_id[20];
    double initial_balance;
} __attribute__((packed)) CreateAccountRequest;

typedef struct {
    char account_id[20];
    double amount;
} __attribute__((packed)) DepositRequest;

typedef struct {
    char account_id[20];
    double amount;
} __attribute__((packed)) WithdrawRequest;

typedef struct {
    char account_id[20];
} __attribute__((packed)) BalanceRequest;

typedef struct {
    int status;
    char message[256];
    double balance;  // For balance query or final balance after operation
} __attribute__((packed)) BankingResponse;

// Protocol Functions
uint16_t calculate_checksum(const char *data, size_t length);
int verify_checksum(const BankingPacket *packet);
int pack_request(BankingPacket *packet, uint16_t opcode, const void *data, size_t data_size);
int unpack_request(const BankingPacket *packet, void *data, size_t data_size);
int pack_response(BankingPacket *packet, const BankingResponse *response);
int unpack_response(const BankingPacket *packet, BankingResponse *response);

#endif // PROTOCOL_H
