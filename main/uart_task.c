#include "freertos/FreeRTOS.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "include/global_config.h"
#include "include/global_tasks.h"
#include "include/global_state_def.h"

#include "include/mive/common.h"
#include "include/mive/psa_packet_defs.h"
#include "include/mive/psa_helper.h"

#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "UART";
static int uart_recv_buffer_num = 0;

IRAM_ATTR static void uart_timer_callback_f(void* user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t uart_queue = (QueueHandle_t)user_data;
    struct mive_global_event timer_event = {
        .ev_data = NULL,
        .event = MIVE_EVENT_TIMER_1MS,
    };
    xQueueSendFromISR(uart_queue, &timer_event, &high_task_wakeup);
    if(high_task_wakeup)
    {
        esp_timer_isr_dispatch_need_yield();
    }
}


static mive_uart_task_packet_t* get_uart_recv_buffer(void)
{
    mive_uart_task_packet_t* to_ret = &global_uart_receive_buffers[uart_recv_buffer_num];

    uart_recv_buffer_num = (uart_recv_buffer_num >= (PSA_MAIN_UART_SEND_BUFFERS_NUM - 1)) ? 0 : uart_recv_buffer_num + 1;

    return to_ret;
}

static void uart_rx_task(void* params);

void uart_task(void* params)
{
    int ret = 0;
    const int uart_buffer_size = 2048;
    struct mive_global_event event = {0};
    QueueHandle_t main_queue = g_global_state.global_main_queue;
    QueueHandle_t tss_queue = g_global_state.global_tss_queue;
    QueueHandle_t van_queue = g_global_state.global_van_queue;
    QueueHandle_t uart_queue = g_global_state.global_uart_queue;
    QueueHandle_t uart_driver_queue;
    esp_timer_handle_t uart_timer_handle;
    mive_uart_task_packet_t* uart_packet = NULL;
    uint8_t* uart_buffer = malloc(256);
    uint8_t* uart_data_buffer = NULL;
    uint8_t uart_crc;
    struct psa_header* psa_packet_header = (struct psa_header*)uart_buffer;


    esp_timer_create_args_t uart_timer_create_args = {
        .arg = uart_queue,
        .callback = uart_timer_callback_f,
        .dispatch_method = ESP_TIMER_ISR,
        .name = NULL,
        .skip_unhandled_events = true
    };
    
    uart_config_t uart_config = {
        .baud_rate = 500000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, uart_buffer_size, 0, 10, &uart_driver_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, 4, 5, -1, -1));

    xTaskCreatePinnedToCore(
        uart_rx_task,
        "uart_rx_task",
        5120, 
        &g_global_state, 11, NULL, xPortGetCoreID());

    esp_timer_create(&uart_timer_create_args, &uart_timer_handle);
    esp_timer_start_once(uart_timer_handle, 1000000);

    while(true)
    {
        ret = xQueueReceive(uart_queue, &event, pdMS_TO_TICKS(500));
        if(ret == pdPASS)
        {
            switch (event.event)
            {
            case MIVE_EVENT_TIMER_UART_SEND:
                // Send data
                esp_timer_start_once(uart_timer_handle, 1000);
                break;
            
            case MIVE_EVENT_UART_SEND:
                uart_packet = (mive_uart_task_packet_t*)event.ev_data;
                if(uart_packet->iden > PSA_MSP_MIN_IDENT && uart_packet->data_size < PSA_MSP_MAX_SIZE)
                {
                    uart_data_buffer = uart_buffer + sizeof(*psa_packet_header);

                    psa_packet_header->ident = uart_packet->iden;
                    psa_packet_header->direction = '>';
                    psa_packet_header->size = uart_packet->data_size;
                    psa_packet_header->start = PSA_MSP_START_MAGIC;

                    memcpy(uart_data_buffer, uart_packet->data, uart_packet->data_size);

                    uart_crc = psa_crc8_checksum(uart_buffer + 2u, uart_packet->data_size + 3u);

                    uart_data_buffer[psa_packet_header->size] = uart_crc;

                    uart_write_bytes(
                        UART_NUM_2, 
                        uart_buffer, 
                        uart_packet->data_size + sizeof(*psa_packet_header) + 1u);
                }
                vTaskDelay(1);
                break;
            default:
                break;
            }
        }
    }

    vTaskDelete(NULL);
    return;
}

void uart_rx_task(void* params)
{
    QueueHandle_t main_queue = g_global_state.global_main_queue;
    QueueHandle_t tss_queue = g_global_state.global_tss_queue;
    QueueHandle_t van_queue = g_global_state.global_van_queue;
    QueueHandle_t uart_queue = g_global_state.global_uart_queue;
    struct mive_global_event global_event = {
        .event = MIVE_EVENT_UART_RECEIVE,
        .ev_data = NULL,
    };
    uint8_t* rx_buffer = malloc(2048);
    uint8_t* data;
    uint8_t calc_crc;
    mive_uart_task_packet_t* task_packet;

    struct psa_header* header = (struct psa_header*)rx_buffer;
    data = rx_buffer + sizeof(*header);

    while(1)
    {
        int rxBytes = uart_read_bytes(UART_NUM_2, rx_buffer, 5, pdMS_TO_TICKS(1000));
        if(rxBytes > 0)
        {
            ESP_LOGD(TAG, "[%s] Got %d bytes", __func__, rxBytes);

            if(rxBytes == sizeof(*header))
            {
                rxBytes = uart_read_bytes(UART_NUM_2, data, header->size + 1, pdMS_TO_TICKS(5));
                calc_crc = psa_crc8_checksum(rx_buffer + 2u, header->size + 3u);
                if(calc_crc == data[header->size])
                {
                    ESP_LOGD(TAG, "[%s] Valid packet\n", __func__);
                    task_packet = get_uart_recv_buffer();
                    task_packet->iden = header->ident;
                    task_packet->data_size = header->size;
                    memcpy(task_packet->data, data, header->size);
                    global_event.ev_data = task_packet;
                    xQueueSendToBack(main_queue, &global_event, 0);
                }
            }
        }
    
    }

}
