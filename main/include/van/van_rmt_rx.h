#ifndef VAN_RMT_RX_H
#define VAN_RMT_RX_H

#include <stdint.h>
#include <stdbool.h>

#include "driver/rmt_rx.h"
#include "freertos/queue.h"

typedef enum
{
    RX_VAN_LINE_LEVEL_LOW = 0,
    RX_VAN_LINE_LEVEL_HIGH = 1,
} RX_VAN_LINE_LEVEL;

typedef enum
{
    RX_VAN_NETWORK_BODY = 0,
    RX_VAN_NETWORK_COMFORT = 1,
} RX_VAN_NETWORK_TYPE;

typedef struct {
    int rmt_rx_ledPin;
    int rmt_rx_rxPin;
    uint8_t rmt_rx_channel;
    uint8_t rmt_rx_time_slice_divisor;
    RX_VAN_LINE_LEVEL rmt_rx_van_line_level;

    rmt_channel_handle_t rx_chan;
    rmt_receive_config_t rx_recv_config;
    QueueHandle_t receive_queue;
    rmt_symbol_word_t *rmt_symbol_buffer;
    size_t rmt_symbol_buffer_size;

} van_rmt_rx_instance_t;

void van_rmt_rx_channel_stop_new(van_rmt_rx_instance_t* instance);
void van_rmt_rx_channel_init_new(
    van_rmt_rx_instance_t* instance, 
    uint8_t channel, 
    int rxPin, 
    int ledPin, 
    RX_VAN_LINE_LEVEL vanLineLevel, 
    RX_VAN_NETWORK_TYPE vanNetworkType);
void van_rmt_rx_channel_start_new(van_rmt_rx_instance_t* instance);

bool van_rmt_rx_parse_byte(van_rmt_rx_instance_t* instance, uint8_t level, uint32_t duration, uint8_t *bitCounter, uint8_t *tempByte, uint8_t *mask, uint8_t *finalByte);
uint16_t van_rmt_rx_crc15(uint8_t data[], uint8_t lengthOfData);
bool van_rmt_rx_is_crc_ok(uint8_t vanMessage[], int vanMessageLength);


#endif // VAN_RMT_RX_H