/*
 * protocol.c
 * Implementation of Custom Banking Protocol
 */

#include "protocol.h"
#include <string.h>
#include <arpa/inet.h>

// CRC16-CCITT Algorithm
uint16_t calculate_checksum(const char *data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    
    return crc;
}

// Verify packet checksum
int verify_checksum(const BankingPacket *packet) {
    size_t data_length = ntohl(packet->packet_length) - PROTOCOL_HEADER_SIZE;
    uint16_t calculated = calculate_checksum(packet->data, data_length);
    return (calculated == ntohs(packet->checksum)) ? 0 : -1;
}

// Pack request into packet
int pack_request(BankingPacket *packet, uint16_t opcode, const void *data, size_t data_size) {
    if (data_size > MAX_DATA_SIZE) {
        return -1;
    }
    
    // Clear packet
    memset(packet, 0, sizeof(BankingPacket));
    
    // Set opcode
    packet->opcode = htons(opcode);
    
    // Copy data
    if (data && data_size > 0) {
        memcpy(packet->data, data, data_size);
    }
    
    // Calculate checksum
    packet->checksum = htons(calculate_checksum(packet->data, data_size));
    
    // Set total length
    packet->packet_length = htonl(PROTOCOL_HEADER_SIZE + data_size);
    
    return 0;
}

// Unpack request from packet
int unpack_request(const BankingPacket *packet, void *data, size_t data_size) {
    // Verify checksum first
    if (verify_checksum(packet) != 0) {
        return -1;
    }
    
    size_t actual_data_size = ntohl(packet->packet_length) - PROTOCOL_HEADER_SIZE;
    
    if (actual_data_size > data_size) {
        return -1;
    }
    
    memcpy(data, packet->data, actual_data_size);
    return 0;
}

// Pack response into packet
int pack_response(BankingPacket *packet, const BankingResponse *response) {
    return pack_request(packet, OP_RESPONSE, response, sizeof(BankingResponse));
}

// Unpack response from packet
int unpack_response(const BankingPacket *packet, BankingResponse *response) {
    return unpack_request(packet, response, sizeof(BankingResponse));
}
