#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// ==========================================
// 1. Operation Codes (業務指令)
// ==========================================
// Client -> Server
#define OP_LOGIN       0x1001  // 登入
#define OP_DEPOSIT     0x2001  // 存款
#define OP_WITHDRAW    0x2002  // 提款
#define OP_BALANCE     0x2003  // 查詢餘額
#define OP_EXIT        0x9999  // 斷線

// Server -> Client (Response Codes)
#define RESP_SUCCESS   0x0000  // 成功
#define RESP_ERR_AUTH  0xE001  // 帳號密碼錯誤
#define RESP_ERR_BAL   0xE002  // 餘額不足
#define RESP_ERR_SYS   0xEFFF  // 系統錯誤

// ==========================================
// 2. 封包結構 (Header + Body)
// ==========================================

// [Header] 固定大小: 12 Bytes
// 結構: [Length (4)] + [OpCode (2)] + [Checksum (2)] + [ReqID (4)]
typedef struct {
    uint32_t length;      // 整個封包長度 (Header + Body)
    uint16_t op_code;     // 操作類型 (如 OP_DEPOSIT)
    uint16_t checksum;    // 簡單完整性校驗 (CRC16 或 Sum)
    uint32_t req_id;      // 請求 ID (Client 生成，用於追蹤 Latency)
} __attribute__((packed)) PacketHeader;

// [Body 1] 登入請求 (Login Payload)
typedef struct {
    char username[32];
    char password[32];
} __attribute__((packed)) LoginRequest;

// [Body 2] 交易請求 (存款/提款)
typedef struct {
    uint32_t account_id;  // 帳號 ID (若是 Session 制可省略，看設計)
    int32_t  amount;      // 金額
} __attribute__((packed)) TransactionRequest;

// [Body 3] 通用回應 (Server 回傳的 Payload)
typedef struct {
    uint16_t status_code; // RESP_SUCCESS 或錯誤碼
    int32_t  balance;     // 當前餘額 (交易後更新)
    char     message[64]; // 文字訊息 (如 "Deposit OK")
} __attribute__((packed)) GeneralResponse;

#endif // PROTOCOL_H