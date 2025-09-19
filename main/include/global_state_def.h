#ifndef MIVE_GLOBAL_STATE_H
#define MIVE_GLOBAL_STATE_H

#include "freertos/queue.h"

#include "global_tasks.h"

#include "van/tss463c.h"
#include "van/van_rmt_rx.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"

struct mive_global_state_t 
{
    // Tasks to VAN
    QueueHandle_t global_van_queue;
    // Tasks to tss
    QueueHandle_t global_tss_queue;
    // Tasks to uart
    QueueHandle_t global_uart_queue;
    // Tasks to main
    QueueHandle_t global_main_queue;

    tss_instance_t *tss_instance;
    van_rmt_rx_instance_t *van_rmt_instance;

    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t cali_handle;

};

extern struct mive_global_state_t g_global_state;
extern volatile int g_global_car_state;
extern volatile float g_bat_voltage;

#endif // MIVE_GLOBAL_STATE_H