#include "freertos/FreeRTOS.h"

#include <string.h>
#include <stdlib.h>

#include "esp_attr.h"
#include "freertos/queue.h"

#include "include/mive/common.h"
#include "include/global_tasks.h"
#include "include/global_state_def.h"
#include "include/global_config.h"

#include "include/van/tss463c.h"
#include "include/van/tss463_regs.h"
#include "include/van/van_packet_iden.h"
#include "esp_log.h"

tss_instance_t global_tss_instance = {0};

static const char *TAG = "tss_task";

enum tss_transmission_status
{
    MIVE_TSS_TX_DONE = 0,
    MIVE_TSS_TX_INPROGRESS,
    MIVE_TSS_TX_FAIL,
};

static uint8_t tss_get_memory_addr(uint16_t iden)
{
    switch (iden)
    {
    case PSA_VAN_IDEN_MFD_STATUS:
        return 00;
        break;
    case PSA_VAN_IDEN_CDCHANGER:
        return 10;
        break;
    default:
        return 90;
        break;
    }
}

static uint8_t tss_get_channel_to_use(uint16_t iden)
{
    switch (iden)
    {
    case PSA_VAN_IDEN_MFD_STATUS:
        return 1;
        break;
    case PSA_VAN_IDEN_CDCHANGER:
        return 2;
        break;
    default:
        return 7;
        break;
    }
}

static int tss_send_frame(tss_instance_t* instance, mive_tss_task_packet_t* packet)
{
    int retval = MIVE_OK;
    uint16_t iden = packet->iden;
    uint8_t memory_addr = tss_get_memory_addr(iden);
    uint8_t channel = tss_get_channel_to_use(iden);
    enum tss_message_type msg_type = packet->message_type;

    switch (msg_type)
    {
    case TSS_TRANSMIT_NOACK:
        retval = tss_transmit_message(
            instance, channel, iden, packet->packet, packet->packet_size,
            memory_addr, 0);
        break;

    case TSS_TRANSMIT:
        retval = tss_transmit_message(
            instance, channel, iden, packet->packet, packet->packet_size,
            memory_addr, 1);
        break;

    case TSS_IMM_REPLY:
        retval = tss_immediate_reply_message(
            instance, channel, iden, packet->packet, packet->packet_size,
            memory_addr);
        break;

    case TSS_DEF_REPLY:
    case TSS_REPLY_REQUEST:
        retval = -MIVE_ERR_NOT_IMPLEMENTED;
        break;
    case TSS_NONE:
    default:
        retval = -MIVE_ERR_INVALID_ARGUMENT;
        break;
    }

    ESP_LOGD(TAG, "[%s] Retval = %d", __func__, retval);

    if(retval == MIVE_OK)
    {
        packet->message_channel = channel;
    }
    else if(retval == -MIVE_ERR_CHANNEL_BUSY)
    {
        // Do nothing
        retval = MIVE_OK;
    }
    else
    {
        packet->message_channel = 0xff;
    }

    return retval;
}

static int tss_check_channel_status(tss_instance_t* instance, mive_tss_task_packet_t* packet)
{
    int retval = -MIVE_ERR_INVALID_ARGUMENT;

    uint8_t channel = packet->message_channel;
    message_length_and_status_register_t reg_value = {0};

    reg_value.Value = tss_get_channel_status_register(instance, channel);

    switch (packet->message_type)
    {
    case TSS_TRANSMIT:
    case TSS_TRANSMIT_NOACK:
    case TSS_IMM_REPLY:
    case TSS_DEF_REPLY:
        if(reg_value.data.CHTx)
        {
            retval = MIVE_TSS_TX_DONE;
        }
        else
        {
            retval = MIVE_TSS_TX_INPROGRESS;
        }
        break;
    case TSS_REPLY_REQUEST:
        if(reg_value.data.CHRx)
        {
            retval = MIVE_TSS_TX_DONE;
        }
        else
        {
            retval = MIVE_TSS_TX_INPROGRESS;
        }
        break;
    default:
        retval = -MIVE_ERR_INVALID_ARGUMENT;
        break;
    }

    if(retval == MIVE_TSS_TX_DONE)
    {
        tss_free_channel(instance, channel);
    }

    return retval;
}

void tss_task(void* params)
{
    int task_running = true;
    int ret = 0;
    mive_global_state_t* state = (mive_global_state_t*) params;
    tss_instance_t* instance = &global_tss_instance;
    struct mive_global_event event_data = {0};
    mive_tss_task_packet_t *task_packet = NULL;
    QueueHandle_t tss_queue = state->global_tss_queue;

    tss_create(instance);

    tss_start(instance);

    while (task_running)
    {
        ret = xQueueReceive(tss_queue, &event_data, pdMS_TO_TICKS(100));
        if(ret == pdPASS)
        {
            ESP_LOGD(TAG, "[%s] Got event from queue: %d", __func__, event_data.event);
            switch (event_data.event)
            {
            case MIVE_EVENT_TSS_WRITE_FRAME:
                ret = tss_send_frame(instance, (mive_tss_task_packet_t*)event_data.ev_data);
                // if(ret == MIVE_OK)
                // {
                //     event_data.event = MIVE_EVENT_TSS_MONITOR_CHANNEL;
                //     xQueueSendToBack(tss_queue, &event_data, 0);
                // }
                break;
                

            case MIVE_EVENT_TSS_MONITOR_CHANNEL:
                ret = tss_check_channel_status(instance, (mive_tss_task_packet_t*)event_data.ev_data);
                if(ret == MIVE_TSS_TX_INPROGRESS)
                {
                    // Send back to queue
                    xQueueSendToBack(tss_queue, &event_data, 0);
                }
                break;
            
            case MIVE_EVENT_TSS_RESET:
                tss_start(instance);
                break;
            
            case MIVE_EVENT_TSS_ACTIVATE:
                tss_activate(instance);
                break;

            case MIVE_EVENT_TSS_IDLE:
                tss_idle(instance);
                break;
            
            case MIVE_EVENT_TSS_SLEEP:
                tss_sleep(instance);
                break;
            default:
                break;
            }
        }
    }
}