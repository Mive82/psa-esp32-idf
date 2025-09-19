#include "freertos/FreeRTOS.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "include/mive/common.h"
#include "include/van/van_rmt_rx.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include "driver/rmt_rx.h"

#include "esp_log.h"

static const char *TAG = "RMT_RX";

#define VAN_CRC_POLYNOM = 0x0F9D;

#define VAN_CRC_TABLE_SIZE 256
static const uint16_t crcTable[VAN_CRC_TABLE_SIZE] = 
{
    0x0000, 0x0F9D, 0x1F3A, 0x10A7, 0x3E74, 0x31E9, 0x214E, 0x2ED3,
    0x7CE8, 0x7375, 0x63D2, 0x6C4F, 0x429C, 0x4D01, 0x5DA6, 0x523B,
    0x764D, 0x79D0, 0x6977, 0x66EA, 0x4839, 0x47A4, 0x5703, 0x589E,
    0x0AA5, 0x0538, 0x159F, 0x1A02, 0x34D1, 0x3B4C, 0x2BEB, 0x2476,
    0x6307, 0x6C9A, 0x7C3D, 0x73A0, 0x5D73, 0x52EE, 0x4249, 0x4DD4,
    0x1FEF, 0x1072, 0x00D5, 0x0F48, 0x219B, 0x2E06, 0x3EA1, 0x313C,
    0x154A, 0x1AD7, 0x0A70, 0x05ED, 0x2B3E, 0x24A3, 0x3404, 0x3B99,
    0x69A2, 0x663F, 0x7698, 0x7905, 0x57D6, 0x584B, 0x48EC, 0x4771,
    0x4993, 0x460E, 0x56A9, 0x5934, 0x77E7, 0x787A, 0x68DD, 0x6740,
    0x357B, 0x3AE6, 0x2A41, 0x25DC, 0x0B0F, 0x0492, 0x1435, 0x1BA8,
    0x3FDE, 0x3043, 0x20E4, 0x2F79, 0x01AA, 0x0E37, 0x1E90, 0x110D,
    0x4336, 0x4CAB, 0x5C0C, 0x5391, 0x7D42, 0x72DF, 0x6278, 0x6DE5,
    0x2A94, 0x2509, 0x35AE, 0x3A33, 0x14E0, 0x1B7D, 0x0BDA, 0x0447,
    0x567C, 0x59E1, 0x4946, 0x46DB, 0x6808, 0x6795, 0x7732, 0x78AF,
    0x5CD9, 0x5344, 0x43E3, 0x4C7E, 0x62AD, 0x6D30, 0x7D97, 0x720A,
    0x2031, 0x2FAC, 0x3F0B, 0x3096, 0x1E45, 0x11D8, 0x017F, 0x0EE2,
    0x1CBB, 0x1326, 0x0381, 0x0C1C, 0x22CF, 0x2D52, 0x3DF5, 0x3268,
    0x6053, 0x6FCE, 0x7F69, 0x70F4, 0x5E27, 0x51BA, 0x411D, 0x4E80,
    0x6AF6, 0x656B, 0x75CC, 0x7A51, 0x5482, 0x5B1F, 0x4BB8, 0x4425,
    0x161E, 0x1983, 0x0924, 0x06B9, 0x286A, 0x27F7, 0x3750, 0x38CD,
    0x7FBC, 0x7021, 0x6086, 0x6F1B, 0x41C8, 0x4E55, 0x5EF2, 0x516F,
    0x0354, 0x0CC9, 0x1C6E, 0x13F3, 0x3D20, 0x32BD, 0x221A, 0x2D87,
    0x09F1, 0x066C, 0x16CB, 0x1956, 0x3785, 0x3818, 0x28BF, 0x2722,
    0x7519, 0x7A84, 0x6A23, 0x65BE, 0x4B6D, 0x44F0, 0x5457, 0x5BCA,
    0x5528, 0x5AB5, 0x4A12, 0x458F, 0x6B5C, 0x64C1, 0x7466, 0x7BFB,
    0x29C0, 0x265D, 0x36FA, 0x3967, 0x17B4, 0x1829, 0x088E, 0x0713,
    0x2365, 0x2CF8, 0x3C5F, 0x33C2, 0x1D11, 0x128C, 0x022B, 0x0DB6,
    0x5F8D, 0x5010, 0x40B7, 0x4F2A, 0x61F9, 0x6E64, 0x7EC3, 0x715E,
    0x362F, 0x39B2, 0x2915, 0x2688, 0x085B, 0x07C6, 0x1761, 0x18FC,
    0x4AC7, 0x455A, 0x55FD, 0x5A60, 0x74B3, 0x7B2E, 0x6B89, 0x6414,
    0x4062, 0x4FFF, 0x5F58, 0x50C5, 0x7E16, 0x718B, 0x612C, 0x6EB1,
    0x3C8A, 0x3317, 0x23B0, 0x2C2D, 0x02FE, 0x0D63, 0x1DC4, 0x1259,
};

IRAM_ATTR static bool rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    van_rmt_rx_instance_t* van_instance = (van_rmt_rx_instance_t*)user_data;
    // send the received RMT symbols to the parser task
    rmt_receive(
        van_instance->rx_chan, 
        van_instance->rmt_symbol_buffer, 
        van_instance->rmt_symbol_buffer_size,
        &van_instance->rx_recv_config);
    xQueueSendFromISR(van_instance->receive_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

IRAM_ATTR bool van_rmt_rx_parse_byte(
    van_rmt_rx_instance_t* instance, 
    uint8_t level, 
    uint32_t duration, 
    uint8_t *bitCounter, 
    uint8_t *tempByte, 
    uint8_t *mask, 
    uint8_t *finalByte)
{
    bool result = false;
    if (instance->rmt_rx_van_line_level == RX_VAN_LINE_LEVEL_LOW)
    {
        level = !level;
    }

    // on the bus the time slices are a little off from the multiple of the TS microseconds, so we round it to the nearest multiple of the TS before dividing
    uint8_t countOfTimeSlices = round_to_nearest(duration, instance->rmt_rx_time_slice_divisor) / instance->rmt_rx_time_slice_divisor;

    for (int i = 0; i < countOfTimeSlices; i++)
    {
        // every 5th bit is a manchester bit, we must skip them while we are building our byte
        bool isManchesterBit = (*bitCounter + 1) % 5 == 0;
        if (!isManchesterBit)
        {
            if (level == 1)
            {
                *tempByte |= *mask;
            }
            *mask = *mask >> 1;
        }
        else
        {
            // if we found the second manchester bit, then we have the full byte in the tempByte variable, so we place it in the finalByte and we can start building the next byte
            if (*bitCounter == 9)
            {
                *bitCounter = -1;
                *mask = 1 << 7;
                *finalByte = *tempByte;
                *tempByte = 0;
                result = true;
            }
        }
        *bitCounter = *bitCounter + 1;
    }

    return result;
}

IRAM_ATTR uint16_t van_rmt_rx_crc15(uint8_t data[], uint8_t lengthOfData)
{
    uint16_t crc15 = 0x7FFF;

    for (int i = 0; i < lengthOfData; i++)  // Skip first byte (SOF, 0x0E) and last 2 (CRC)
    {
        uint8_t byte = data[i];

        // XOR-in next input byte into MSB of crc, that's our new intermediate divident
        uint8_t pos = (uint8_t)( (crc15 >> 7) ^ byte);

        // Shift out the MSB used for division per lookup table and XOR with the remainder
        crc15 = (uint16_t)((crc15 << 8) ^ (uint16_t)(crcTable[pos]));
    } // for

    crc15 ^= 0x7FFF;
    crc15 <<= 1;  // Shift left 1 bit to turn 15 bit result into 16 bit representation

    return crc15;
}

// IRAM_ATTR uint16_t van_rmt_rx_crc15(uint8_t data[], uint8_t lengthOfData)
// {
//     const uint8_t order = 15;
//     const uint16_t polynom = 0xF9D;
//     const uint16_t xorValue = 0x7FFF;
//     const uint16_t mask = 0x7FFF;

//     uint16_t crc = 0x7FFF;

//     for (uint8_t i = 0; i < lengthOfData; i++)
//     {
//         uint8_t currentByte = data[i];

//         // rotate one data byte including crcmask
//         for (uint8_t j = 0; j < 8; j++)
//         {
//             bool bit = (crc & (1 << (order - 1))) != 0;
//             if ((currentByte & 0x80) != 0)
//             {
//                 bit = !bit;
//             }
//             currentByte <<= 1;

//             crc = ((crc << 1) & mask) ^ (-bit & polynom);
//         }
//     }

//     // perform xor and multiply result by 2 to turn 15 bit result into 16 bit representation
//     return (crc ^ xorValue) << 1;
// }

IRAM_ATTR bool van_rmt_rx_is_crc_ok(uint8_t vanMessage[], int vanMessageLength)
{
    bool retval = false;
    uint8_t crcByte1 = 0;
    uint8_t crcByte2 = 0;
    uint16_t crcValueInMessage = 0;

    // Check if message length is even valid (Start byte + iden + crc)
    if(vanMessageLength < 5)
    {
        return false;
    }

    crcByte1 = vanMessage[vanMessageLength - 2];
    crcByte2 = vanMessage[vanMessageLength - 1];
    crcValueInMessage = crcByte1 << 8 | crcByte2;

    if (vanMessageLength - 3 <= 32)
    {
        uint16_t calculatedCrc = van_rmt_rx_crc15(vanMessage + 1, vanMessageLength - 3);
        if(crcValueInMessage == calculatedCrc)
        {
            retval = true;
        }
        else
        {
            ESP_LOGE(TAG, "[%s] Calculated: 0x%04x, Received: 0x%04x", __func__, calculatedCrc, crcValueInMessage);
            retval = false;
        }
    }
    return retval;
}

/* Uninstall the RMT driver */
void van_rmt_rx_channel_stop_new(van_rmt_rx_instance_t* instance)
{
    rmt_disable(instance->rx_chan);
}

void van_rmt_rx_channel_start_new(van_rmt_rx_instance_t* instance)
{
    rmt_enable(instance->rx_chan);
}

void van_rmt_rx_channel_init_new(
    van_rmt_rx_instance_t* instance, 
    uint8_t channel, 
    int rxPin, 
    int ledPin,
    RX_VAN_LINE_LEVEL vanLineLevel, 
    RX_VAN_NETWORK_TYPE vanNetworkType)
{
    rmt_rx_channel_config_t rx_chan_config = {0};
    rmt_receive_config_t rx_recv_config = {0};
    rmt_channel_handle_t rx_chan;
    uint32_t timeslot_interval_ns = 0;
    uint8_t _rmt_van_rx_time_slice_divisor;
    QueueHandle_t receive_queue = NULL;
    rmt_symbol_word_t* rmt_symbol_buffer = NULL;

    // TS = 8us = 125k
    // TS = 16us = 62.5k

    if(rxPin < 0)
    {
        ESP_LOGE(TAG, "[%s] Invalid rxPin: %d", __func__, rxPin);
        return;
    }

    rx_chan_config.gpio_num = rxPin;
    rx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rx_chan_config.resolution_hz = 1000000; // 1 MHz
    rx_chan_config.mem_block_symbols = 512;
    // RMT DMA not supported on ESP32
    rx_chan_config.flags.with_dma = 0;

    if (vanNetworkType == RX_VAN_NETWORK_COMFORT)
    {
        _rmt_van_rx_time_slice_divisor = 8;
        timeslot_interval_ns = 8 * 1000;
    }
    else
    {
        _rmt_van_rx_time_slice_divisor = 16;
        timeslot_interval_ns = 16 * 1000;
    }

    rx_recv_config.signal_range_max_ns = 10 * timeslot_interval_ns;
    rx_recv_config.signal_range_min_ns = timeslot_interval_ns / _rmt_van_rx_time_slice_divisor;

    rmt_new_rx_channel(&rx_chan_config, &rx_chan);

    receive_queue = xQueueCreate(30, sizeof(rmt_rx_done_event_data_t));

    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = rmt_rx_done_callback
    };
    rmt_rx_register_event_callbacks(rx_chan, &cbs, instance);

    rmt_symbol_buffer = heap_caps_malloc(2048, MALLOC_CAP_DMA);

    instance->rmt_rx_channel = channel;
    instance->rmt_rx_rxPin = rxPin;
    instance->rmt_rx_van_line_level = vanLineLevel;
    instance->rmt_rx_time_slice_divisor = _rmt_van_rx_time_slice_divisor;
    instance->rx_chan = rx_chan;
    instance->rx_recv_config = rx_recv_config;
    instance->receive_queue = receive_queue;
    instance->rmt_symbol_buffer = rmt_symbol_buffer;
    instance->rmt_symbol_buffer_size = 2048;
}
