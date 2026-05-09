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
#include "include/mive/psa_helper.h"
#include "driver/gpio.h"
#include "esp_log.h"

tss_instance_t global_tss_instance = {0};

static int tss_task_buffer_num = 0;

static const char *TAG = "tss_task";

QueueHandle_t tss_internal_queue = NULL;

struct tss_message_config
{
    uint16_t iden;
    enum tss_message_type message_type;
    uint8_t memory_addr;
    uint8_t channel_num;
};

enum tss_transmission_status
{
    MIVE_TSS_CHANNEL_FREE = 0,
    MIVE_TSS_TX_DONE = 1,
    MIVE_TSS_RX_DONE = 2,
    MIVE_TSS_IN_PROGRESS,
    MIVE_TSS_FAIL,
};

struct tss_message_config message_configs[] = {
    {.iden = 0x8c4, .message_type = TSS_RECEIVE, .memory_addr = 0, .channel_num = 1},
    {.iden = 0x4d4, .message_type = TSS_REPLY_REQUEST, .memory_addr = 4, .channel_num = 2},
    {.iden = 0x554, .message_type = TSS_REPLY_REQUEST, .memory_addr = 16, .channel_num = 3},
    {.iden = 0x564, .message_type = TSS_REPLY_REQUEST, .memory_addr = 36, .channel_num = 4},
    {.iden = 0x8d4, .message_type = TSS_TRANSMIT, .memory_addr = 66, .channel_num = 5},
    {.iden = 0x5e4, .message_type = TSS_TRANSMIT, .memory_addr = 76, .channel_num = 6},
};

static int tss_check_channel_status(tss_instance_t* instance, uint16_t iden);

IRAM_ATTR static void tss_monitor_timer_callback_f(void* user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t)user_data;
    struct mive_global_event timer_event = {
        .ev_data = {0},
        .event = MIVE_EVENT_TSS_MONITOR_CHANNEL,
    };
    xQueueSendFromISR(queue, &timer_event, &high_task_wakeup);
    if(high_task_wakeup)
    {
        esp_timer_isr_dispatch_need_yield();
    }
}

IRAM_ATTR static void tss_interrupt(void* user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    tss_instance_t* instance = (tss_instance_t*)user_data;
    interrupt_register_t it_register;
    struct mive_global_event timer_event = {
        .ev_data = {0},
        .event = MIVE_EVENT_TSS_MONITOR_CHANNEL,
    };

    tss_register_get(instance, TSS_INTERRUPTSTATUS, &(it_register.Value));
    tss_register_set(instance, TSS_INTERRUPTRESET, it_register.Value);

    if(it_register.data.ROK)
    {
        xQueueSendFromISR(tss_internal_queue, &timer_event, NULL);
    }
}

static uint8_t tss_get_memory_addr(uint16_t iden)
{
    int i = 0;

    for (i = 0; i < sizeof(message_configs) / sizeof(*message_configs); ++i)
    {
        if(iden == message_configs[i].iden)
        {
            return message_configs[i].memory_addr;
        }
    }

    return 90;
}

static uint8_t tss_get_channel_to_use(uint16_t iden)
{
    int i = 0;

    for (i = 0; i < sizeof(message_configs) / sizeof(*message_configs); ++i)
    {
        if(iden == message_configs[i].iden)
        {
            return message_configs[i].channel_num;
        }
    }

    return 7;
}

static uint8_t tss_get_message_type(uint16_t iden)
{
    int i = 0;

    for (i = 0; i < sizeof(message_configs) / sizeof(*message_configs); ++i)
    {
        if(iden == message_configs[i].iden)
        {
            return message_configs[i].message_type;
        }
    }

    return TSS_NONE;
}

static int tss_send_frame(tss_instance_t* instance, mive_tss_task_packet_t* packet)
{
    int retval = MIVE_OK;
    int ret = 0;
    uint16_t iden = packet->iden;
    uint8_t memory_addr = tss_get_memory_addr(iden);
    uint8_t channel = tss_get_channel_to_use(iden);
    enum tss_message_type msg_type = packet->message_type;
    enum tss_message_type expected_type = tss_get_message_type(iden);

    ret = tss_check_channel_status(instance, iden);

    if(msg_type != expected_type)
    {
        ESP_LOGW(TAG, "Invalid message type for iden %03x. Expected %d, got %d", iden, expected_type, msg_type);
        msg_type = expected_type;
    }

    if(ret > MIVE_TSS_RX_DONE)
    {
        if(msg_type == TSS_TRANSMIT)
        {
            // Queue the packet internally again
            xQueueSendToBack(tss_internal_queue, &packet, 0);
            return MIVE_OK;
        }
    }

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

    case TSS_REPLY_REQUEST:
        retval = tss_reply_request_message(instance, channel, iden, memory_addr, packet->packet_size, 1);
        break;

    case TSS_RECEIVE:
        retval = tss_receive_message(instance, channel, iden, memory_addr, packet->packet_size);
        break;

    case TSS_DEF_REPLY:
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

static int tss_check_channel_status(tss_instance_t* instance, uint16_t iden)
{
    int retval = -MIVE_ERR_INVALID_ARGUMENT;
    uint8_t channel = tss_get_channel_to_use(iden);
    int message_type = tss_get_message_type(iden);
    message_length_and_status_register_t reg_value = {0};

    if(tss_is_channel_free(instance, channel))
    {
        return MIVE_TSS_CHANNEL_FREE;
    }

    reg_value.Value = tss_get_channel_status_register(instance, channel);

    // ESP_LOGI(TAG, "CH %d - CHTx: %d", channel, reg_value.data.CHTx);

    switch (message_type)
    {
    case TSS_TRANSMIT:
    case TSS_TRANSMIT_NOACK:
    case TSS_IMM_REPLY:
    case TSS_DEF_REPLY:
        {
            if(reg_value.data.CHTx)
            {
                retval = MIVE_TSS_TX_DONE;
            }
            else
            {
                retval = MIVE_TSS_IN_PROGRESS;
            }
        }
        break;
    case TSS_REPLY_REQUEST:
    case TSS_RECEIVE:
        {
            if(reg_value.data.CHRx)
            {
                retval = MIVE_TSS_RX_DONE;
            }
            else
            {
                retval = MIVE_TSS_IN_PROGRESS;
            }
        }
        break;
    default:
        retval = -MIVE_ERR_INVALID_ARGUMENT;
        break;
    }

    if(retval == MIVE_TSS_RX_DONE || retval == MIVE_TSS_TX_DONE)
    {
        tss_free_channel(instance, channel);
    }

    return retval;
}

static void tss_process_internal_queue(tss_instance_t* instance)
{
    int ret = 0;
    mive_tss_task_packet_t queue_data = {0};

    ret = xQueueReceive(tss_internal_queue, &queue_data, 0);
    if(ret == pdPASS)
    {
        tss_send_frame(instance, &queue_data);
    }
}

mive_tss_task_packet_t* get_tss_task_buffer(void)
{
    mive_tss_task_packet_t* to_ret = &global_tss_buffers[tss_task_buffer_num];

    tss_task_buffer_num = (tss_task_buffer_num + 1) % PSA_MAIN_TSS_BUFFERS_NUM;

    return to_ret;
}

void tss_task(void* params)
{
    int task_running = true;
    int ret = 0;
    int mess_idx = 0;
    mive_global_state_t* state = (mive_global_state_t*) params;
    tss_instance_t* instance = &global_tss_instance;
    struct mive_global_event event_data = {0};
    QueueHandle_t tss_queue = state->global_tss_queue;
    esp_timer_handle_t tss_timer_handle = NULL;
    tss_internal_queue = xQueueCreate(10, sizeof(mive_tss_task_packet_t));

    esp_timer_create_args_t timer_create_args = {
        .arg = tss_queue,
        .callback = tss_monitor_timer_callback_f,
        .dispatch_method = ESP_TIMER_ISR,
        .name = NULL,
        .skip_unhandled_events = true
    };

    esp_timer_create(&timer_create_args, &tss_timer_handle);
    esp_timer_start_once(tss_timer_handle, 500000);

    tss_create(instance);

    tss_start(instance);

    gpio_set_direction(TSS_INT_PIN, GPIO_MODE_INPUT);

    // gpio_set_intr_type(TSS_INT_PIN, GPIO_INTR_LOW_LEVEL);

    // gpio_isr_handler_add(TSS_INT_PIN, tss_interrupt, instance);

    while (task_running)
    {
        ret = xQueueReceive(tss_queue, &event_data, pdMS_TO_TICKS(100));
        if(ret == pdPASS)
        {
            ESP_LOGD(TAG, "[%s] Got event from queue: %d", __func__, event_data.event);
            switch (event_data.event)
            {
            case MIVE_EVENT_TSS_WRITE_FRAME:
                ret = tss_send_frame(instance, event_data.ev_data.tss_event_data);
                // if(ret == MIVE_OK)
                // {
                //     event_data.event = MIVE_EVENT_TSS_MONITOR_CHANNEL;
                //     xQueueSendToBack(tss_queue, &event_data, 0);
                // }
                break;
            // Timer event
            case MIVE_EVENT_TSS_MONITOR_CHANNEL:
                {
                    uint16_t iden = message_configs[mess_idx].iden;
                    // Process the internal queue for any backed up messages
                    tss_process_internal_queue(instance);

                    // Check if any channels need to be setup again
                    ret = tss_check_channel_status(instance, iden);

                    // Setup the receive messages automatically
                    if(ret <= MIVE_TSS_RX_DONE && message_configs[mess_idx].message_type == TSS_RECEIVE)
                    {
                        emf_receive(iden, 3);
                    }

                    ret = esp_timer_restart(tss_timer_handle, 50000);
                    if(ret != ESP_OK)
                    {
                        esp_timer_start_once(tss_timer_handle, 50000);
                    }

                    mess_idx = (mess_idx + 1) % (sizeof(message_configs) / sizeof(*message_configs));
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
            taskYIELD();
        }
    }
}