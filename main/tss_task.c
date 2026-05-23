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

static portMUX_TYPE tss_task_spinlock = portMUX_INITIALIZER_UNLOCKED;

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
    MIVE_TSS_FAIL,
    MIVE_TSS_IN_PROGRESS,
};

// Packets that we need the TSS to ACK
struct tss_message_config const message_configs[] = {
    {.iden = 0x4d4, .message_type = TSS_REPLY_REQUEST, .memory_addr = 5,   .channel_num = 1},
    {.iden = 0x554, .message_type = TSS_REPLY_REQUEST, .memory_addr = 17,  .channel_num = 2},
    {.iden = 0x564, .message_type = TSS_REPLY_REQUEST, .memory_addr = 47,  .channel_num = 3},
    {.iden = 0x5e4, .message_type = TSS_TRANSMIT,      .memory_addr = 97,  .channel_num = 4},
    {.iden = 0x8c4, .message_type = TSS_RECEIVE,       .memory_addr = 0,   .channel_num = 5},
    {.iden = 0x8d4, .message_type = TSS_TRANSMIT,      .memory_addr = 77,  .channel_num = 6},
    {.iden = 0x9c4, .message_type = TSS_RECEIVE,       .memory_addr = 101, .channel_num = 7},
};

static int tss_check_channel_status(tss_instance_t* instance, uint16_t iden);

IRAM_ATTR static void tss_monitor_timer_callback_f(void* user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t)user_data;
    struct mive_global_event timer_event = {
        .ev_data = {0},
        .event = MIVE_EVENT_TSS_INTERRUPT,
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
    tss_instance_t* instance = (tss_instance_t*)&global_tss_instance;
    QueueHandle_t tss_queue = (QueueHandle_t)user_data;
    interrupt_register_t it_register = {0};
    last_message_status_register_t lms_register = {0};
    struct mive_global_event timer_event = {
        .ev_data = {0},
        .event = MIVE_EVENT_TSS_INTERRUPT,
    };

    tss_register_get(instance, TSS_INTERRUPTSTATUS, &(it_register.Value));
    tss_register_get(instance, TSS_LASTMESSAGESTATUS, &(lms_register.Value));
    tss_register_set(instance, TSS_INTERRUPTRESET, it_register.Value);

    // Ignore the Reset interrupt
    if(it_register.Value & 0x7f)
    {
        timer_event.ev_data.tss_interrupt_data.channel_number = lms_register.data.IDTr;
        timer_event.ev_data.tss_interrupt_data.interrupt_status_reg = it_register.Value;
        xQueueSendToFrontFromISR(tss_queue, &timer_event, &high_task_wakeup);
    }

    if(high_task_wakeup)
    {
        portYIELD_FROM_ISR();
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

    return 106;
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

    return 12;
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

int tss_send_frame(mive_tss_task_packet_t* packet)
{
    tss_instance_t* instance = &global_tss_instance;
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

    if(ret > MIVE_TSS_FAIL)
    {
        if(msg_type == TSS_TRANSMIT)
        {
            // Queue the packet internally again
            xQueueSendToBack(tss_internal_queue, packet, 0);
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
        retval = tss_receive_message(instance, channel, iden, packet->packet_size, memory_addr);
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

    if(reg_value.data.CHER)
    {
        retval = MIVE_TSS_FAIL;
    }
    else {
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
        case TSS_REPLY_REQUEST:
            {
                if(reg_value.data.CHTx)
                {
                    // Transmit part is done
                    if(reg_value.data.CHRx)
                    {
                        retval = MIVE_TSS_RX_DONE;
                    }
                    // All frames I request are in-frame replies,
                    // so treat this case as a failure
                    else
                    {
                        retval = MIVE_TSS_FAIL;
                    }
                }
                else
                {
                    retval = MIVE_TSS_IN_PROGRESS;
                }
            }
        default:
            retval = -MIVE_ERR_INVALID_ARGUMENT;
            break;
        }
    }

    if(retval != MIVE_TSS_IN_PROGRESS)
    {
        tss_free_channel(instance, channel);
    }

    return retval;
}

static void tss_process_internal_queue()
{
    int ret = 0;
    mive_tss_task_packet_t queue_data = {0};

    ret = xQueueReceive(tss_internal_queue, &queue_data, 0);
    if(ret == pdPASS)
    {
        tss_send_frame(&queue_data);
    }
}

static int tss_get_frame(tss_instance_t* instance, struct tss_message_config* message, mive_tss_task_packet_t* output)
{
    uint8_t data_size = 0;
    uint8_t regvals[32] = {0};
    uint8_t memory_addr = message->memory_addr + 0x80;
    uint16_t iden = message->iden;
    uint8_t regval = 0;

    tss_register_get(instance, memory_addr, &regval);

    data_size = regval & 0x1f;

    ESP_LOGI(TAG, "Got %03x(%d)", iden, data_size);
    if(data_size > 0 && data_size < 30)
    {
        tss_registers_get(instance, memory_addr + 1, output->packet, data_size);

        output->iden = iden;
        output->packet_size = data_size;
        return MIVE_OK;
    }
    else{
        memset(output, 0, sizeof(*output));
        return -MIVE_ERR_VAN_INVALID_PACKET_SIZE;
    }
}

mive_tss_task_packet_t* get_tss_task_buffer(void)
{
    vPortEnterCriticalSafe(&tss_task_spinlock);
    mive_tss_task_packet_t* to_ret = &global_tss_buffers[tss_task_buffer_num];

    tss_task_buffer_num = (tss_task_buffer_num + 1) % PSA_MAIN_TSS_BUFFERS_NUM;

    vPortExitCriticalSafe(&tss_task_spinlock);
    return to_ret;
}

void tss_init(void* params)
{
    mive_global_state_t* state = (mive_global_state_t*) params;
    tss_instance_t* instance = &global_tss_instance;
    tss_internal_queue = xQueueCreate(10, sizeof(mive_tss_task_packet_t));

    tss_create(instance);

    tss_start(instance);

    gpio_set_direction(TSS_INT_PIN, GPIO_MODE_INPUT);

    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_intr_type(TSS_INT_PIN, GPIO_INTR_LOW_LEVEL));

    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_isr_handler_add(TSS_INT_PIN, tss_interrupt, state->global_main_queue));

}

static int tss_get_interrupt_type(interrupt_register_t regval)
{
    int int_type = 0;
    if(regval.data.RNOK || regval.data.ROK)
    {
        return TSS_RECEIVE;
    }

    if(regval.data.TOK)
    {
        return TSS_TRANSMIT;
    }
}

void tss_process_interrupt(mive_tss_interrupt_packet_t interrupt_data)
{
    int ret = 0;
    unsigned int idx = 0;
    tss_instance_t* instance = &global_tss_instance;
    mive_tss_task_packet_t* task_packet = NULL;
    interrupt_register_t int_reg = {.Value = interrupt_data.interrupt_status_reg};
    uint8_t channel = interrupt_data.channel_number;
    uint16_t iden = 0;
    for(unsigned int i = 0; i < (sizeof(message_configs) / sizeof(*message_configs)); ++i)
    {
        if(message_configs[i].channel_num == channel)
        {
            iden = message_configs[i].iden;
            idx = i;
            break;
        }
    }

    if(iden == 0)
    {
        return;
    }
    ESP_LOGI(TAG, "Interrupt caused by channel %d - %x", channel, iden);
    // Process the internal queue for any backed up messages

    switch (message_configs[idx].message_type)
    {
    case TSS_REPLY_REQUEST:
    case TSS_RECEIVE:
        if(!int_reg.data.RE){

            task_packet = get_tss_task_buffer();
            ret = tss_get_frame(instance, &message_configs[idx], task_packet);
            if(ret == MIVE_OK)
            {
                psa_parse_van_packet(
                    task_packet->iden, task_packet->packet_size, task_packet->packet, &global_libpsa_buffers);
            }
        }
        break;
    case TSS_TRANSMIT:
    case TSS_TRANSMIT_NOACK:
        if(!int_reg.data.TE)
        {
            // Handle transmit sequence here
            tss_process_internal_queue();
        }
        break;
    default:
        break;
    }

    switch (iden)
    {
    case 0x8c4:
        emf_receive(0x8c4, 4);
        break;
    case 0x9c4:
        emf_receive(0x9c4, 3);
        break;
    default:
        break;
    }

}