#ifndef MIVE_COMMON_H
#define MIVE_COMMON_H

#include <stdint.h>

typedef enum {
    MIVE_OK = 0,
    MIVE_ERR,
    MIVE_ERR_INVALID_ARGUMENT,
    MIVE_ERR_NOT_IMPLEMENTED,
    MIVE_ERR_OUTOFMEMORY,
    MIVE_ERR_VAN_CRC_ERROR,
    MIVE_ERR_TSS_BUSY,
    MIVE_ERR_CHANNEL_BUSY,
    MIVE_ERR_VAN_UNKNOWN_IDEN,
    MIVE_ERR_VAN_INVALID_PACKET_SIZE,
} mive_error;

uint8_t round_to_nearest(uint8_t num_to_round, uint8_t multiple);
uint16_t get_iden_from_bytes(uint8_t const byte1, uint8_t const byte2);
uint8_t dec_to_bcd(uint8_t input);
uint8_t bcd_to_dec(uint8_t value);
uint8_t psa_crc8_checksum(const uint8_t * ptr, uint32_t len);


#endif // MIVE_COMMON_H