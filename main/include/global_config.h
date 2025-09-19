#ifndef MIVE_GLOBAL_CONFIG_H
#define MIVE_GLOBAL_CONFIG_H

// #include "soc/gpio_num.h"


#define DEVKIT 1
#define PEZO 2

// #define ESP32_BOARD_TYPE PEZO
#define ESP32_BOARD_TYPE PEZO

#if (ESP32_BOARD_TYPE == PEZO)

#define VAN_RX_PIN GPIO_NUM_21
#define VAN_RX_LED_PIN -1
#define TSS_CS_PIN 22
#define TSS_INT_PIN 19
#define PSA_ADC_UNIT 0
#define PSA_ADC_CHANNEL 5
#define PSA_EXT_REG_PIN 27
#define TSS_OE_ENABLE_PIN 32

#elif (ESP32_BOARD_TYPE == DEVKIT)

#define VAN_RX_PIN GPIO_NUM_21
#define VAN_RX_LED_PIN GPIO_NUM_2
#define TSS_CS_PIN 5
#define TSS_INT_PIN 27
#define PSA_ADC_UNIT 0
#define PSA_ADC_CHANNEL 5
#define PSA_EXT_REG_PIN -1
#define TSS_OE_ENABLE_PIN -1

#else

#define VAN_RX_PIN -1
#define VAN_RX_LED_PIN -1
#define TSS_CS_PIN -1
#define TSS_INT_PIN -1
#define PSA_ADC_UNIT -1
#define PSA_ADC_CHANNEL -1
#define PSA_EXT_REG_PIN -1
#define TSS_OE_ENABLE_PIN -1

#endif

#endif // GLOBAL_CONFIG_H