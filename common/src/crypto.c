#include "crypto.h"

// 預設實作：簡單的字節加總 (Sum Check)
// 安全性負責人可以在這裡改成 CRC16 或其他演算法
uint16_t calculate_checksum(const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint16_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += bytes[i];
    }
    return sum;
}

int verify_checksum(uint16_t received_sum, const void *data, size_t len) {
    uint16_t calculated = calculate_checksum(data, len);
    return (received_sum == calculated);
}