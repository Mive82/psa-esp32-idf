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
    uint8_t radio_state_user;
    uint8_t radio_source;
    uint8_t radio_source_target;
    int8_t radio_setting_volume;
    int8_t radio_setting_bass;
    int8_t radio_setting_treble;
    int8_t radio_setting_balance;
    int8_t radio_setting_fader;
    uint8_t radio_setting_auto_vol;
    uint8_t radio_setting_loudness;
    uint8_t radio_menu_state;
    uint8_t radio_554_state;
    uint8_t radio_mute;

    uint8_t keyboard_override;
    uint8_t radio_cd_present;
    uint8_t accessory;
    uint8_t ignition;

    struct {
        uint8_t volume_minus : 1;
        uint8_t volume_plus : 1;
        uint8_t source : 1;
        uint8_t seek_fwd : 1;
        uint8_t seek_bwd : 1;
        uint8_t : 3;
    } stalk_buttons;
    uint8_t stalk_wheel;
    uint8_t radio_buttons[0x40];
};

// Radio state that should be preserved in sleep
struct mive_radio_rtc_state_t
{
    uint8_t radio_state_user;
    uint8_t radio_source_target;
    int8_t radio_setting_volume;
    int8_t radio_setting_bass;
    int8_t radio_setting_treble;
    int8_t radio_setting_balance;
    int8_t radio_setting_fader;
    uint8_t radio_setting_auto_vol;
    uint8_t radio_setting_loudness;
};

struct mive_fuel_state_t
{
    uint32_t fuel_cons_total;   // Total fuel used in this session. Expressed in 1E-4 liters
    uint32_t distance_total_dm; // Total distance travelled in this session.
    uint16_t fuel_cons;         // Fuel used since last calculation. Expressed in 1E-4 liters
    uint16_t dist_dm;           // Distance in decimeters covered since last calculation.
    uint16_t dist_dm_last;      // Last value encountered in packets
    uint8_t fuel_cons_last;     // Last value encountered in packets
};

extern struct mive_global_state_t g_global_state;
extern struct mive_radio_state_t g_radio_state;
extern struct mive_fuel_state_t g_fuel_state;
extern volatile uint8_t g_global_car_state;
extern volatile float g_bat_voltage;

#endif // MIVE_GLOBAL_STATE_H