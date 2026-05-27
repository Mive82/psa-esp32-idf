#include "freertos/FreeRTOS.h"
#include <stdint.h>
#include <string.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali_scheme.h"

#include "include/global_init.h"
#include "include/mive/common.h"
#include "include/global_config.h"
#include "include/global_state_def.h"
#include "include/mive/psa_helper.h"

#include "soc/soc_caps.h"


void init_adc()
{
    adc_oneshot_unit_handle_t adc1_handle;
    adc_cali_handle_t cali_handle;
    adc_atten_t atten_val;

    if(ESP32_BOARD_TYPE == PEZOV2)
    {
        // Divider configuration allows up to 30 V.
        // On Atten = 2, effective measurement is to around 1700 mV,
        // or around 17 V with the divider. Max stable voltage I'm expecting
        // is around 14.5 V
        atten_val = ADC_ATTEN_DB_6;
    }
    else{
        atten_val = ADC_ATTEN_DB_12;
    }

    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = PSA_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = atten_val,
    };

    adc_cali_line_fitting_config_t lf_config = {
        .atten = atten_val,
        .bitwidth = ADC_BITWIDTH_12,
        .default_vref = 0,
        .unit_id = PSA_ADC_UNIT
    };

    //ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_c_handle));

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_5, &config));

    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&lf_config, &cali_handle));

    g_global_state.adc_handle = adc1_handle;
    g_global_state.cali_handle = cali_handle;
}


void init_libpsa_packets(void)
{
    memset(&global_libpsa_buffers, 0, sizeof(global_libpsa_buffers));

    global_libpsa_buffers.engine_data = calloc(1, sizeof(struct psa_engine_data));
    global_libpsa_buffers.radio_data = calloc(1, sizeof(struct psa_radio_data));
    global_libpsa_buffers.presets_data_am = calloc(1, sizeof(struct psa_preset_data));
    global_libpsa_buffers.presets_data_fm_1 = calloc(1, sizeof(struct psa_preset_data));
    global_libpsa_buffers.presets_data_fm_2 = calloc(1, sizeof(struct psa_preset_data));
    global_libpsa_buffers.presets_data_fm_ast = calloc(1, sizeof(struct psa_preset_data));
    global_libpsa_buffers.headunit_data = calloc(1, sizeof(struct psa_headunit_data));
    global_libpsa_buffers.cd_player_data = calloc(1, sizeof(struct psa_cd_player_data));
    global_libpsa_buffers.vin_data = calloc(1, 17);
    global_libpsa_buffers.dash_data = calloc(1, sizeof(struct psa_dash_data));
    global_libpsa_buffers.door_data = calloc(1, sizeof(struct psa_door_data));
    global_libpsa_buffers.trip_data = calloc(1, sizeof(struct psa_trip_data));
    global_libpsa_buffers.status_data = calloc(1, sizeof(struct psa_status_data));
    global_libpsa_buffers.fuel_data = calloc(1, sizeof(struct psa_fuel_data));

    global_uart_send_buffers = calloc(PSA_MAIN_UART_SEND_BUFFERS_NUM, sizeof(*global_uart_send_buffers));
    global_uart_receive_buffers = calloc(PSA_MAIN_UART_SEND_BUFFERS_NUM, sizeof(*global_uart_send_buffers));
    global_tss_buffers = calloc(PSA_MAIN_TSS_BUFFERS_NUM, sizeof(*global_tss_buffers));
}
