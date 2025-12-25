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

// CRC16-CCITT Algorithm
// uint16_t calculate_checksum(const char *data, size_t length) {
//     uint16_t crc = 0xFFFF;
    
//     for (size_t i = 0; i < length; i++) {
//         crc ^= (uint16_t)data[i] << 8;
//         for (int j = 0; j < 8; j++) {
//             if (crc & 0x8000)
//                 crc = (crc << 1) ^ 0x1021;
//             else
//                 crc <<= 1;
//         }
//     }
    
//     return crc;
// }
int verify_checksum(uint16_t received_sum, const void *data, size_t len) {
    uint16_t calculated = calculate_checksum(data, len);
    return (received_sum == calculated);
}