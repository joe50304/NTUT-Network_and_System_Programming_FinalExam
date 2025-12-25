/*
 * protocol.c
 * Implementation of Custom Banking Protocol
 */

#include "protocol.h"
#include "crypto.h"
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h> // for rand()

// Verify packet checksum
int verify_packet_checksum(const BankingPacket *packet) {
    // 修改：透過 header 存取 length
    size_t data_length = ntohl(packet->header.length) - PROTOCOL_HEADER_SIZE;
    
    // 計算 Data 的 checksum
    uint16_t calculated = calculate_checksum(packet->data, data_length);
    
    // 修改：透過 header 存取 checksum
    return (calculated == ntohs(packet->header.checksum)) ? 0 : -1;
}

// Pack request into packet
int pack_request(BankingPacket *packet, uint16_t opcode, const void *data, size_t data_size) {
    if (data_size > MAX_DATA_SIZE) {
        return -1;
    }
    
    memset(packet, 0, sizeof(BankingPacket));
    
    // 修改：設定 Header 欄位
    packet->header.op_code = htons(opcode);
    packet->header.req_id = rand(); // 簡單給個 ID
    
    if (data && data_size > 0) {
        memcpy(packet->data, data, data_size);
    }
    
    // 修改：計算 Checksum 並存入 header
    packet->header.checksum = htons(calculate_checksum(packet->data, data_size));
    
    // 修改：設定 Length
    packet->header.length = htonl(PROTOCOL_HEADER_SIZE + data_size);
    
    return 0;
}

// Unpack request from packet
int unpack_request(const BankingPacket *packet, void *data, size_t data_size) {
    if (verify_packet_checksum(packet) != 0) {
        return -1;
    }
    
    // 修改：透過 header 取得長度
    size_t actual_data_size = ntohl(packet->header.length) - PROTOCOL_HEADER_SIZE;
    
    if (actual_data_size > data_size) {
        return -1;
    }
    
    memcpy(data, packet->data, actual_data_size);
    return 0;
}

int pack_response(BankingPacket *packet, const BankingResponse *response) {
    return pack_request(packet, OP_RESPONSE, response, sizeof(BankingResponse));
}

int unpack_response(const BankingPacket *packet, BankingResponse *response) {
    return unpack_request(packet, response, sizeof(BankingResponse));
}
