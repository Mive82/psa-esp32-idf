#ifndef _VAN_REMOTE_STALK_STRUCT_H
#define _VAN_REMOTE_STALK_STRUCT_H

#include <stdint.h>

struct van_9c4_byte1_struct {
    uint8_t                           : 1; // bit 0
    uint8_t source                    : 1; // bit 1
    uint8_t volume_minus              : 1; // bit 2
    uint8_t volume_plus               : 1; // bit 3
    uint8_t counter_overflow_negative : 1; // bit 4
    uint8_t counter_overflow_positive : 1; // bit 5
    uint8_t seek_down                 : 1; // bit 6
    uint8_t seek_up                   : 1; // bit 7
};

struct van_radio_remote_struct {
    union {
        struct van_9c4_byte1_struct data;
        uint8_t byte;
    } button_status;
    uint8_t scroll_position;
};

#endif 