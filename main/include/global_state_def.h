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

struct mive_radio_state_t
{
    uint8_t economy_mode;
    // Radio stuff
    uint8_t radio_state_current;
    uint8_t radio_state_target;
    uint8_t radio_state_ignition;
    uint8_t radio_source;
    int8_t radio_setting_volume;
    int8_t radio_setting_bass;
    int8_t radio_setting_treble;
    int8_t radio_setting_balance;
    int8_t radio_setting_fader;
    uint8_t radio_setting_auto_vol;
    uint8_t radio_setting_loudness;
    uint8_t radio_menu_state;
    uint8_t radio_554_state;

    uint8_t keyboard_override;
    uint8_t radio_cd_present;
    uint8_t accessory;
    uint8_t ignition;
};

extern struct mive_global_state_t g_global_state;
extern struct mive_radio_state_t g_radio_state;
extern volatile uint8_t g_global_car_state;
extern volatile float g_bat_voltage;

#endif // MIVE_GLOBAL_STATE_H