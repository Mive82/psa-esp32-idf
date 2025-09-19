#include "freertos/FreeRTOS.h"

#include <string.h>
#include <stdlib.h>

#include "esp_attr.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "include/mive/common.h"
#include "include/global_tasks.h"
#include "include/global_state_def.h"
#include "include/global_config.h"
#include "include/van/van_rmt_rx.h"

van_rmt_rx_instance_t global_van_instance;

#define VAN_MAX_NUM_PACKETS 50

static uint8_t* buffers[VAN_MAX_NUM_PACKETS];

static int van_parse_bytes(van_rmt_rx_instance_t* instance, mive_van_packet_t* packet, rmt_rx_done_event_data_t rx_data, int packet_index)
{
    rmt_symbol_word_t* items;

    uint8_t bitCounter = 0;
    uint8_t mask = 1 << 7;
    uint8_t tempByte = 0;
    uint8_t finalByte = 0;
    int van_message_length = 0;
    size_t i = 0;
    bool isCompleteByte = false;
    int retval = 0;

    uint8_t temp_array[VAN_MAX_PACKET_LEN + 10] = {0};

    items = rx_data.received_symbols;
    // printf("Got %d symbols\n", rx_data.num_symbols);
    for (i = 0; i < rx_data.num_symbols; i++)
    {
        if(van_message_length >= VAN_MAX_PACKET_LEN)
        {
            break;
        }
        isCompleteByte = van_rmt_rx_parse_byte(instance, items[i].level0, items[i].duration0, &bitCounter, &tempByte, &mask, &finalByte);
        if (isCompleteByte)
        {
            temp_array[van_message_length] = finalByte;
            van_message_length++;
        }

        isCompleteByte = van_rmt_rx_parse_byte(instance, items[i].level1, items[i].duration1, &bitCounter, &tempByte, &mask, &finalByte);
        if (isCompleteByte)
        {
            temp_array[van_message_length] = finalByte;
            van_message_length++;
        }
    }
    
    // printf("[%s] Received: ", __func__);
    // for(int i = 0; i < van_message_length; ++i)
    // {
    //     printf("0x%02x ", temp_array[i]);
    // }

    // printf("\n");

    if((van_message_length > 5) && 
       (van_message_length < VAN_MAX_PACKET_LEN) && 
       (van_rmt_rx_is_crc_ok(temp_array, van_message_length)))
    {
        packet->iden = (((uint16_t)temp_array[1] << 8) | ((uint16_t)temp_array[2] & 0xf0)) >> 4;
        packet->packet_size = van_message_length - 5;
        packet->packet = buffers[packet_index];
        memset(packet->packet, 0, packet->packet_size);
        memcpy(packet->packet, temp_array + 3, packet->packet_size);
        retval = MIVE_OK;
    }
    else
    {
        // printf("[%s] CRC error\n", __func__);
        retval = -MIVE_ERR_VAN_CRC_ERROR;
    }
    //  0e 5e 48 a0 20 11 d4 

    return retval;
}

void van_rmt_task(void* params)
{
    int ret = 0;
    unsigned int packet_num = 0;
    mive_global_state_t *state = (mive_global_state_t*)params;
    state->van_rmt_instance = &global_van_instance;
    van_rmt_rx_instance_t* van_instance = &global_van_instance;
    mive_van_packet_t* van_packets = NULL;
    QueueHandle_t receive_queue;
    QueueHandle_t global_queue_to_main;
    rmt_rx_done_event_data_t rx_data;
    struct mive_global_event event_item_to_send = {.event = MIVE_EVENT_RMT_NEW_VAN_FRAME};

    printf("[%s] Starting...\n", __func__);

    van_rmt_rx_channel_init_new(
        van_instance, 
        0, 
        VAN_RX_PIN, 
        VAN_RX_LED_PIN, 
        RX_VAN_LINE_LEVEL_HIGH,
        RX_VAN_NETWORK_COMFORT);


    van_rmt_rx_channel_start_new(van_instance);

    rmt_receive(
        van_instance->rx_chan, 
        van_instance->rmt_symbol_buffer, 
        van_instance->rmt_symbol_buffer_size,
        &van_instance->rx_recv_config);

    receive_queue = van_instance->receive_queue;
    global_queue_to_main = state->global_main_queue;
    
    van_packets = calloc(VAN_MAX_NUM_PACKETS, sizeof(*van_packets));

    for(int i = 0; i < VAN_MAX_NUM_PACKETS; ++i)
    {
        buffers[i] = calloc(1, VAN_MAX_PACKET_LEN);
    }

    while (true)
    {
        if(xQueueReceive(receive_queue, &rx_data, pdMS_TO_TICKS(1000)) == pdPASS)
        {
            ret = van_parse_bytes(van_instance, &van_packets[packet_num], rx_data, packet_num);
            if(ret == MIVE_OK)
            {
                event_item_to_send.ev_data = &van_packets[packet_num];
                // printf("[%s] Sending to queue...\n", __func__);
                xQueueSendToBack(global_queue_to_main, &event_item_to_send, 0);

                packet_num = (packet_num == VAN_MAX_NUM_PACKETS - 1) ? 0 : packet_num + 1;
            }
        }
    }
    
}
