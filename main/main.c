#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include <string.h>

#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "include/global_config.h"
#include "include/global_tasks.h"
#include "include/global_state_def.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"
#include "ulp.h"
#include "ulp_adc.h"
#include "ulp_common.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "driver/uart.h"

#include "esp_timer.h"

#include "include/mive/common.h"
#include "include/global_init.h"
#include "include/van/tss463c.h"
#include "include/van/van_packet_iden.h"
#include "include/van_structs/van_cdc_emulator_structs.h"
#include "include/van/van_packet_parser.h"
#include "include/mive/psa_packet_defs.h"

#include "include/mive/psa_helper.h"


#define ARRAY_SIZE_OFFSET   5
mive_global_state_t g_global_state = {0};
volatile int g_global_car_state = PSA_STATE_CAR_SLEEP;
volatile int g_global_ext_power_state = 1;
volatile float g_bat_voltage = 0.0f;

static const char *TAG = "MAIN";

static const float ADC_R2 = 523000;
static const float ADC_R1 = 2000000;
static const int audio_menu_duration_ms = 4000;

static int timer_num = 0;

struct psa_output_data_buffers global_libpsa_buffers;

mive_uart_task_packet_t* global_uart_send_buffers;
mive_uart_task_packet_t* global_uart_receive_buffers;


RTC_DATA_ATTR int rtc_data_valid = 0;
RTC_DATA_ATTR struct psa_preset_data rtc_preset_data[4];

void main_task(void* params);
void stats_task(void *arg);

static char* car_state_str[] = {
    [PSA_STATE_CAR_SLEEP] = __STRINGIFY(PSA_STATE_CAR_SLEEP),
    [PSA_STATE_ECONOMY_MODE] = __STRINGIFY(PSA_STATE_ECONOMY_MODE),
    [PSA_STATE_CAR_LOCKED] = __STRINGIFY(PSA_STATE_CAR_LOCKED),
    [PSA_STATE_CAR_OFF] = __STRINGIFY(PSA_STATE_CAR_OFF),
    [PSA_STATE_RADIO_ON] = __STRINGIFY(PSA_STATE_RADIO_ON),
    [PSA_STATE_ACCESSORY] = __STRINGIFY(PSA_STATE_ACCESSORY),
    [PSA_STATE_IGNITION] = __STRINGIFY(PSA_STATE_IGNITION),
    [PSA_STATE_CRANKING] = __STRINGIFY(PSA_STATE_CRANKING),
    [PSA_STATE_ENGINE_ON] = __STRINGIFY(PSA_STATE_ENGINE_ON),
    [PSA_STATE_UNKNOWN] = __STRINGIFY(PSA_STATE_UNKNOWN)
};

IRAM_ATTR static void timer_callback_f(void* user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t main_queue = (QueueHandle_t)user_data;
    struct mive_global_event timer_event = {
        .ev_data = NULL,
        .event = MIVE_EVENT_TIMER_1S,
    };
    xQueueSendFromISR(main_queue, &timer_event, &high_task_wakeup);
    if(high_task_wakeup)
    {
        esp_timer_isr_dispatch_need_yield();
    }
}

IRAM_ATTR static void timer_callback_audio_timeout_f(void* user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t main_queue = (QueueHandle_t)user_data;
    struct mive_global_event timer_event = {
        .ev_data = NULL,
        .event = MIVE_EVENT_VAN_CLOSE_AUDIO_MENU,
    };
    xQueueSendFromISR(main_queue, &timer_event, &high_task_wakeup);
    if(high_task_wakeup)
    {
        esp_timer_isr_dispatch_need_yield();
    }
}

IRAM_ATTR static void timer_callback_oneshot_f(void* user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t main_queue = (QueueHandle_t)user_data;
    struct mive_global_event timer_event = {
        .ev_data = NULL,
        .event = MIVE_EVENT_TIMER_1MS,
    };
    timer_num++;
    xQueueSendFromISR(main_queue, &timer_event, &high_task_wakeup);
    if(high_task_wakeup)
    {
        esp_timer_isr_dispatch_need_yield();
    }
}

static esp_err_t print_real_time_stats(TickType_t xTicksToWait)
{
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    configRUN_TIME_COUNTER_TYPE start_run_time, end_run_time;
    esp_err_t ret;

    //Allocate array to store current task states
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = malloc(sizeof(TaskStatus_t) * start_array_size);
    if (start_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get current task states
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    vTaskDelay(xTicksToWait);

    //Allocate array to store tasks states post delay
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = malloc(sizeof(TaskStatus_t) * end_array_size);
    if (end_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get post delay task states
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    //Calculate total_elapsed_time in units of run time stats clock period.
    uint32_t total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    printf("| Task | Run Time | Percentage\n");
    //Match each task in start_array to those in the end_array
    for (int i = 0; i < start_array_size; i++) {
        int k = -1;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                k = j;
                //Mark that task have been matched by overwriting their handles
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                break;
            }
        }
        //Check if matching task found
        if (k >= 0) {
            uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
            uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * CONFIG_FREERTOS_NUMBER_OF_CORES);
            printf("| %s | %"PRIu32" | %"PRIu32"%%\n", start_array[i].pcTaskName, task_elapsed_time, percentage_time);
        }
    }

    //Print unmatched tasks
    for (int i = 0; i < start_array_size; i++) {
        if (start_array[i].xHandle != NULL) {
            printf("| %s | Deleted\n", start_array[i].pcTaskName);
        }
    }
    for (int i = 0; i < end_array_size; i++) {
        if (end_array[i].xHandle != NULL) {
            printf("| %s | Created\n", end_array[i].pcTaskName);
        }
    }
    ret = ESP_OK;

exit:    //Common return path
    free(start_array);
    free(end_array);
    return ret;
}

/**
 * @brief Setup the ULP as a wake source to wake the ESP up
 * when the ADC reading goes above `high_adc_treshold`.
 * 
 * @param high_adc_treshold 
 */
void ulp_adc_wake_up(unsigned int high_adc_treshold)
{
    esp_err_t err;
    adc_oneshot_unit_handle_t adc1_handle = g_global_state.adc_handle;
    
    ulp_adc_cfg_t adc_cfg = {
        .adc_n = PSA_ADC_UNIT,
        .channel = PSA_ADC_CHANNEL,
        .width = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
        .ulp_mode = ADC_ULP_MODE_FSM
    };
    
    const ulp_insn_t program[] = {
        I_MOVI(R0, 0),                // Set reg. R0 to initial 0
        I_MOVI(R2, 0),                // Set reg. R2 to initial 0
        M_LABEL(1),                   // LABEL 1 - Start of measurement
        I_ADDI(R0, R0, 1),            // Increment cycle counter (reg. R0)
        I_ADC(R1, PSA_ADC_UNIT, PSA_ADC_CHANNEL),            // Read ADC value to reg. R1
        I_ADDR(R2, R2, R1),           // Add ADC value from reg R1 to reg. R2
        M_BL(1, 4),                   // If cycle counter is less than 4, go to LABEL 1
        I_RSHI(R0, R2, 2),            // Divide accumulated ADC value in reg. R2 by 4 and save it to reg. R0
        M_BGE(2, high_adc_treshold),  // If average ADC value from reg. R0 is higher or equal than high_adc_treshold, go to LABEL 2
        I_HALT(),                     // Halt the coprocessor, it will be woken up by the timer again
        M_LABEL(2),                   // LABEL 3
        I_WAKE(),                     // Wake up ESP32
        I_END(),                      // Stop ULP program timer
        I_HALT(),                     // Halt the coprocessor
    };
    
    adc_oneshot_del_unit(adc1_handle);

    ESP_ERROR_CHECK(ulp_adc_init(&adc_cfg));

    rtc_gpio_init(GPIO_NUM_33);


    size_t size = sizeof(program)/sizeof(ulp_insn_t);
    ESP_ERROR_CHECK(ulp_process_macros_and_load(0, program, &size));
    ulp_set_wakeup_period(0, 20000);
    err = ulp_run(0);
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_sleep_enable_ulp_wakeup());
}

void go_to_sleep(void)
{
    tss_sleep(&global_tss_instance);
    gpio_set_level(32, 0);
    gpio_set_level(27, 0);
    rtc_gpio_isolate(GPIO_NUM_12);
    rtc_gpio_isolate(GPIO_NUM_15);
    printf("Going to sleep\n");
    ulp_adc_wake_up(1000);
    esp_deep_sleep_start();
}

void update_ext_power_state(int power)
{
    // gpio_set_level(PSA_EXT_REG_PIN, power ? 1 : 0);
    // gpio_set_level(TSS_OE_ENABLE_PIN, power ? 1 : 0);

    g_global_ext_power_state = power;
}

void update_car_state(int new_state)
{
    if(new_state == g_global_car_state)
    {
        return;
    }

    ESP_LOGI(TAG, "[%s] New state = %s", __func__, car_state_str[new_state]);
    g_global_car_state = new_state;
    global_libpsa_buffers.status_data->car_state = g_global_car_state;
    libpsa_send_packet(PSA_IDENT_CAR_STATUS);
}

void calculate_car_state(void)
{
    // ESP_LOGI(TAG, "[%s]", __func__);
    int ext_power_status = g_global_ext_power_state;
    int engine_state = global_libpsa_buffers.dash_data->engine_running;
    int acc_state = global_libpsa_buffers.dash_data->accesories_on;
    int ign_state = global_libpsa_buffers.dash_data->ignition_on;
    int headunit_state = global_libpsa_buffers.headunit_data->unit_powered_on;
    int lock_state = global_libpsa_buffers.status_data->doors_locked;

    // PSA_STATE_ENGINE_ON
    if (engine_state)
    {
        update_car_state(PSA_STATE_ENGINE_ON);
        ext_power_status = 1;
    }
    // PSA_STATE_ECONOMY_MODE
    // else if (dash_packet.packet.data.economy_mode)
    // {
    //     update_car_state(PSA_STATE_ECONOMY_MODE);
    //     ext_power_status = 0;
    // }
    // PSA_STATE_CRANKING
    else if (acc_state == 0 &&
             ign_state == 1)
    {
        update_car_state(PSA_STATE_CRANKING);
        ext_power_status = 1;
    }
    // PSA_STATE_IGNITION
    else if (acc_state == 1 &&
             ign_state == 1)
    {
        update_car_state(PSA_STATE_IGNITION);
        ext_power_status = 1;
    }
    // PSA_STATE_ACCESSORY
    else if (acc_state == 1 &&
             ign_state == 0)
    {
        update_car_state(PSA_STATE_ACCESSORY);
        ext_power_status = 1;
    }
    // PSA_STATE_RADIO_ON
    // PSA_STATE_CAR_LOCKED
    // PSA_STATE_CAR_OFF
    else if (acc_state == 0 &&
             ign_state == 0)
    {
        if (headunit_state)
        {
            update_car_state(PSA_STATE_RADIO_ON);
            ext_power_status = 1;
        }
        else if (lock_state)
        {
            update_car_state(PSA_STATE_CAR_LOCKED);
            ext_power_status = 0;
        }
        else
        {
            update_car_state(PSA_STATE_CAR_OFF);
        }
    }

    update_ext_power_state(ext_power_status);
}

void app_main(void)
{
    // To TSS task
    QueueHandle_t tss_queue = xQueueCreate(10, sizeof(struct mive_global_event));
    
    // To VAN task
    QueueHandle_t van_queue = xQueueCreate(10, sizeof(struct mive_global_event));
    
    // To main task
    QueueHandle_t main_queue = xQueueCreate(20, sizeof(struct mive_global_event));

    // To uart task
    QueueHandle_t uart_queue = xQueueCreate(10, sizeof(struct mive_global_event));
    
    g_global_state.global_tss_queue = tss_queue;
    g_global_state.global_van_queue = van_queue;
    g_global_state.global_main_queue = main_queue;
    g_global_state.global_uart_queue = uart_queue;

    init_adc();
    
    init_libpsa_packets();
    
    if(rtc_data_valid)
    {
        memcpy(global_libpsa_buffers.presets_data_am, &rtc_preset_data[PSA_PRESET_AM], sizeof(struct psa_preset_data));
        memcpy(global_libpsa_buffers.presets_data_fm_1, &rtc_preset_data[PSA_PRESET_FM_1], sizeof(struct psa_preset_data));
        memcpy(global_libpsa_buffers.presets_data_fm_2, &rtc_preset_data[PSA_PRESET_FM_2], sizeof(struct psa_preset_data));
        memcpy(global_libpsa_buffers.presets_data_fm_ast, &rtc_preset_data[PSA_PRESET_FMAST], sizeof(struct psa_preset_data));
    }

    xTaskCreatePinnedToCore(
        main_task,
        "main_task",
        10240, 
        NULL, 20, NULL, 0);

    xTaskCreatePinnedToCore(
        tss_task,
        "tss_task",
        5120, 
        &g_global_state, 10, NULL, 0);

    xTaskCreatePinnedToCore(
        van_rmt_task,
        "van_rx_task",
        5120, 
        &g_global_state, 15, NULL, 1);

    xTaskCreatePinnedToCore(
        uart_task,
        "uart_task",
        5120, 
        &g_global_state, 15, NULL, 0);
    
    // xTaskCreatePinnedToCore(stats_task, "stats", 4096, NULL, 3, NULL, tskNO_AFFINITY);
}


void stats_task(void *params)
{
    //Print real time stats periodically
    while (1) {
        printf("\n\nGetting real time stats over %"PRIu32" ticks\n", pdMS_TO_TICKS(1000));
        if (print_real_time_stats(pdMS_TO_TICKS(1000)) == ESP_OK) {
            printf("Real time stats obtained\n");
        } else {
            printf("Error getting real time stats\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void main_task(void* params)
{
    struct mive_global_event event = {0};
    struct mive_global_event tss_event = {0};
    struct mive_global_event tss_trip_event = {0};
    struct van_cd_changer_packet* cdc_packet;
    mive_tss_task_packet_t *tss_packet = malloc(sizeof(*tss_packet));
    mive_tss_task_packet_t *tss_trip_packet = malloc(sizeof(*tss_packet));
    int ret = 0;
    int count = 0;
    int i;
    int num_timer = 0;
    int num_timer_ms = 0;
    int audio_menu_open = 0;
    int audio_menu_setting = 0;
    QueueHandle_t main_queue = g_global_state.global_main_queue;
    QueueHandle_t tss_queue = g_global_state.global_tss_queue;
    QueueHandle_t van_queue = g_global_state.global_van_queue;
    QueueHandle_t uart_queue = g_global_state.global_uart_queue;
    adc_oneshot_unit_handle_t adc1_handle = g_global_state.adc_handle;
    adc_cali_handle_t cali_handle = g_global_state.cali_handle;
    
    esp_timer_handle_t timer_handle;
    esp_timer_handle_t timer_handle_oneshot;
    esp_timer_handle_t timer_handle_audio_timeout;

    uint32_t ret_num = 0;
    mive_van_packet_t* van_packet = NULL;
    mive_uart_queue_packet_t* uart_queue_packet = NULL;
    mive_uart_task_packet_t* uart_recv_packet = NULL;

#if (ESP32_BOARD_TYPE == PEZO)
    esp_rom_gpio_pad_select_gpio(PSA_EXT_REG_PIN);
    esp_rom_gpio_pad_select_gpio(TSS_OE_ENABLE_PIN);

    gpio_set_direction(PSA_EXT_REG_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(TSS_OE_ENABLE_PIN, GPIO_MODE_OUTPUT);
    
    gpio_set_level(PSA_EXT_REG_PIN, 1);
    gpio_set_level(TSS_OE_ENABLE_PIN, 1);

#endif


    tss_packet->iden = PSA_VAN_IDEN_CDCHANGER;
    tss_packet->packet_size = sizeof(*cdc_packet);
    tss_packet->message_type = TSS_IMM_REPLY;
    tss_packet->message_channel = 2;

    tss_trip_packet->iden = PSA_VAN_IDEN_MFD_STATUS; // 0x5E4
    tss_trip_packet->packet_size = 2;
    tss_trip_packet->message_type = TSS_TRANSMIT;
    tss_trip_packet->message_channel = 1;
    tss_trip_packet->packet[0] = 0x00;
    tss_trip_packet->packet[1] = 0xFF;

    cdc_packet = (struct van_cd_changer_packet*)tss_packet->packet;

    memset(cdc_packet, 0, sizeof(*cdc_packet));

    cdc_packet->cd_flag = 1;
    cdc_packet->cd_num = 1;
    cdc_packet->cd_present = VAN_CDC_CD_PRESENT;
    cdc_packet->header = 0x80;
    cdc_packet->footer = 0x80;
    cdc_packet->minutes = 1;
    cdc_packet->seconds = 1;
    cdc_packet->shuffle = 0;
    cdc_packet->status = VAN_CDC_ON_PLAYING;
    cdc_packet->total_tracks = 1;
    cdc_packet->track_num = 1;

    esp_timer_create_args_t timer_create_args = {
        .arg = g_global_state.global_main_queue,
        .callback = timer_callback_f,
        .dispatch_method = ESP_TIMER_ISR,
        .name = NULL,
        .skip_unhandled_events = true
    };

    esp_timer_create_args_t timer_create_args_oneshot = {
        .arg = g_global_state.global_main_queue,
        .callback = timer_callback_oneshot_f,
        .dispatch_method = ESP_TIMER_ISR,
        .name = NULL,
        .skip_unhandled_events = true
    };

    esp_timer_create_args_t timer_create_args_audio_menu = {
        .arg = g_global_state.global_main_queue,
        .callback = timer_callback_audio_timeout_f,
        .dispatch_method = ESP_TIMER_ISR,
        .name = NULL,
        .skip_unhandled_events = true
    };

    esp_timer_create(&timer_create_args, &timer_handle);
    esp_timer_create(&timer_create_args_oneshot, &timer_handle_oneshot);
    esp_timer_create(&timer_create_args_audio_menu, &timer_handle_audio_timeout);
    esp_timer_start_periodic(timer_handle, 1000000);
    esp_timer_start_once(timer_handle_oneshot, 1000000);

    int adc_raw;
    int voltage_in;
    float voltage_out;
    while(1)
    {
        // vTaskDelay(1000 / portTICK_PERIOD_MS);

        ret = xQueueReceive(main_queue, &event, pdMS_TO_TICKS(1000));

        if(ret == pdPASS)
        {
            // printf("[%s] Received queue item\n", __func__);

            switch (event.event)
            {
            case MIVE_EVENT_RMT_NEW_VAN_FRAME:
                {
                    int stat_idx = 0;
                    van_packet = (mive_van_packet_t*)event.ev_data;

                    ret = psa_parse_van_packet(
                        van_packet->iden,
                        van_packet->packet_size,
                        van_packet->packet,
                        &global_libpsa_buffers);

                    if (ret != MIVE_OK)
                    {
                        ESP_LOGE(TAG, "Error parsing packet 0x%03x", van_packet->iden);
                    }

                    // calculate_car_state();
                    // printf("[%s] Received: 0x%3x ", __func__, van_packet->iden);
                    // for (int i = 0; i < van_packet->packet_size; i++)
                    // {
                    //     printf("%02x ", van_packet->packet[i]);
                    // }
                    // printf("\n");
                }

                vTaskDelay(pdMS_TO_TICKS(10));
                break;
            
            case MIVE_EVENT_TIMER_1S:
                {
                    uint8_t temp_val = cdc_packet->header;
                    temp_val = (temp_val >= 0x87) ? 0x80 : temp_val + 1;
                    cdc_packet->header = temp_val;
                    cdc_packet->footer = temp_val;
                    tss_event.event = MIVE_EVENT_TSS_WRITE_FRAME;
                    tss_event.ev_data = tss_packet;
                    xQueueSendToBack(tss_queue, &tss_event, pdMS_TO_TICKS(100));
                }
                break;
            case MIVE_EVENT_TIMER_1MS:

                ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_5, &adc_raw));
                ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, adc_raw, &voltage_in));
                
                g_bat_voltage = (voltage_in * 0.001f * (ADC_R1 + ADC_R2)) / ADC_R2;
                global_libpsa_buffers.status_data->voltage = (uint16_t)(g_bat_voltage * 1000);
                libpsa_send_packet(PSA_IDENT_CAR_STATUS);
                esp_timer_start_once(timer_handle_oneshot, 50000);

                if (unlikely(g_bat_voltage < 1))
                {
                    struct mive_global_event sleep_event = {
                        .ev_data = NULL,
                        .event = MIVE_EVENT_GLOBAL_SLEEP
                    };

                    xQueueSendToFront(main_queue, &sleep_event, 0);
                }

                break;
            case MIVE_EVENT_VAN_NEXT_AUDIO_MENU_ITEM:
                if(!audio_menu_open)
                {
                    esp_timer_start_once(timer_handle_audio_timeout, audio_menu_duration_ms * 1000);
                }
                if (audio_menu_setting >= 6)
                {
                    audio_menu_open = 0;
                    audio_menu_setting = 0;
                    esp_timer_stop(timer_handle_audio_timeout);
                }
                else
                {
                    audio_menu_open = 1;
                    audio_menu_setting++;
                    esp_timer_restart(timer_handle_audio_timeout, audio_menu_duration_ms * 1000);
                }
                ESP_LOGI(TAG, "[MIVE_EVENT_VAN_NEXT_AUDIO_MENU_ITEM] menu_open: %d, menu_setting: %d" , audio_menu_open, audio_menu_setting);
                libpsa_update_audio_settings_packet(audio_menu_open, audio_menu_setting);
                libpsa_send_packet(PSA_IDENT_HEADUNIT);
                break;
            case MIVE_EVENT_VAN_UPDATE_AUDIO_MENU:
                if(audio_menu_open)
                {
                    esp_timer_restart(timer_handle_audio_timeout, audio_menu_duration_ms * 1000);
                    ESP_LOGI(TAG, "[MIVE_EVENT_VAN_UPDATE_AUDIO_MENU] menu_open: %d, menu_setting: %d" , audio_menu_open, audio_menu_setting);
                    libpsa_update_audio_settings_packet(audio_menu_open, audio_menu_setting);
                    libpsa_send_packet(PSA_IDENT_HEADUNIT);
                }
                break;                
            case MIVE_EVENT_VAN_CLOSE_AUDIO_MENU:
                audio_menu_open = 0;
                audio_menu_setting = 0;
                ESP_LOGI(TAG, "[MIVE_EVENT_VAN_CLOSE_AUDIO_MENU] menu_open: %d, menu_setting: %d" , audio_menu_open, audio_menu_setting);
                libpsa_update_audio_settings_packet(audio_menu_open, audio_menu_setting);
                libpsa_send_packet(PSA_IDENT_HEADUNIT);
                break;
            case MIVE_EVENT_UART_NEW_DATA:
                uart_queue_packet = (mive_uart_queue_packet_t*)event.ev_data;

                for(i = 0; i < uart_queue_packet->num_idens; ++i)
                {
                    libpsa_send_packet(uart_queue_packet->idens[i]);
                }
                break;
            case MIVE_EVENT_UART_RECEIVE:
                uart_recv_packet = (mive_uart_task_packet_t*)event.ev_data;

                if(uart_recv_packet->iden >= PSA_MSP_MIN_IDENT && uart_recv_packet->iden < PSA_IDENT_SET_CD_CHANGER_DATA)
                {
                    ESP_LOGI(TAG, "Sending iden 0x%04x", uart_recv_packet->iden);
                    libpsa_send_packet(uart_recv_packet->iden);
                } else if(uart_recv_packet->iden == PSA_IDENT_SET_TRIP_RESET)
                {
                    ESP_LOGI(TAG, "Sending Trip reset");
                    struct psa_trip_reset_data* data = (struct psa_trip_reset_data*)uart_recv_packet->data;
                    tss_trip_event.event = MIVE_EVENT_TSS_WRITE_FRAME;
                    tss_trip_event.ev_data = tss_trip_packet;
                    tss_trip_packet->packet_size = 2;

                    switch (data->trip_meter)
                    {
                    case PSA_TRIP_A:
                        tss_trip_packet->packet[0] = 0xA0;
                        xQueueSendToBack(tss_queue, &tss_trip_event, 0);
                        break;
                    case PSA_TRIP_B:
                        tss_trip_packet->packet[0] = 0x60;
                        xQueueSendToBack(tss_queue, &tss_trip_event, 0);
                        break;
                    default:
                        break;
                    }
                }

                break;
            case MIVE_EVENT_GLOBAL_SLEEP:
                memcpy(&rtc_preset_data[PSA_PRESET_AM], global_libpsa_buffers.presets_data_am, sizeof(struct psa_preset_data));
                memcpy(&rtc_preset_data[PSA_PRESET_FM_1], global_libpsa_buffers.presets_data_fm_1, sizeof(struct psa_preset_data));
                memcpy(&rtc_preset_data[PSA_PRESET_FM_2], global_libpsa_buffers.presets_data_fm_2, sizeof(struct psa_preset_data));
                memcpy(&rtc_preset_data[PSA_PRESET_FMAST], global_libpsa_buffers.presets_data_fm_ast, sizeof(struct psa_preset_data));
                rtc_data_valid = 1;
                go_to_sleep();
                break;
            default:
                break;
            }

        }
        calculate_car_state();
    }
}
