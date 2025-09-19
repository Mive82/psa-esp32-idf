#include "freertos/FreeRTOS.h"
#include <stdint.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali_scheme.h"

#include "include/global_init.h"
#include "include/mive/common.h"
#include "include/global_config.h"
#include "include/global_state_def.h"

#include "soc/soc_caps.h"


void init_adc()
{
    adc_oneshot_unit_handle_t adc1_handle;
    adc_cali_handle_t cali_handle;

    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = PSA_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };

    adc_cali_line_fitting_config_t lf_config = {
        .atten = ADC_ATTEN_DB_12,
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
