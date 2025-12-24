#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>
#include <stdint.h>

/**
 * [安全性同學請修改這裡]
 * 計算 Checksum (完整性檢查)
 * 目前實作：簡單的加總 (Simple Sum)
 * 目標實作：CRC16 或 CRC32
 */
uint16_t calculate_checksum(const void *data, size_t len);

/**
 * [安全性同學請修改這裡]
 * 驗證 Checksum
 * return: 1 = 通過, 0 = 失敗
 */
int verify_checksum(uint16_t received_sum, const void *data, size_t len);

#endif // CRYPTO_H