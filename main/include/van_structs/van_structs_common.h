#ifndef _MIVE_VAN_STRUCTS_COMMON_H
#define _MIVE_VAN_STRUCTS_COMMON_H

#include <stdint.h>

struct psa_radio_settings_values
{
    uint8_t value : 7; // Add 0x3f to get actual value
    uint8_t updating : 1;
} __attribute__((packed));

#endif // _MIVE_VAN_STRUCTS_COMMON_H