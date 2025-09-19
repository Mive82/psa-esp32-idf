#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include <stdint.h>
#include <endian.h>
#include <string.h>

#include "include/mive/common.h"
#include "include/van/van_packet_iden.h"
#include "include/van/van_packet_parser.h"
#include "include/mive/common.h"
#include "include/mive/psa_packet_defs.h"
#include "include/van_structs.h"
#include "include/global_state_def.h"

#define PACKET_BUFFER_NUM 20

static int audio_menu_open = 0;
static int audio_menu_setting = 0; // 0, 1, 2, 3, 4, 5, 6

// static const int audio_menu_duration_ms = 4000;

mive_uart_queue_packet_t packet_buffers[PACKET_BUFFER_NUM];

static int packet_buffer_num = 0;

union dash_mileage
{
    uint32_t number : 24;
    uint8_t buffer[3];
} __attribute__((packed));

static uint16_t swap_endian_uint16(uint16_t const x)
{
    return (x << 8) | (x >> 8);
}

static uint32_t swap_endian_uint32(uint32_t const x)
{
    uint32_t val = ((x << 8) & 0xFF00FF00) | ((x >> 8) & 0xFF00FF);
    return (val << 16) | (val >> 16);
}

static uint32_t mileage_swap_bytes(union dash_mileage const mileage)
{
    uint32_t temp_mileage = (uint32_t)mileage.buffer[0] << 16 | (uint32_t)mileage.buffer[1] << 8 | mileage.buffer[2];
    return temp_mileage;
}

static mive_uart_queue_packet_t* get_packet_buffer(void)
{
    mive_uart_queue_packet_t* to_ret = &packet_buffers[packet_buffer_num];

    packet_buffer_num = (packet_buffer_num >= (PACKET_BUFFER_NUM - 1)) ? 0 : packet_buffer_num + 1;

    return to_ret;
}

static int psa_parse_buttons_iden(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    struct mive_global_event event = {0};

    if (data_buffers->headunit_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    if ((data[1] & 0x0F) == 0x02)
    {
        // Refresh the timer when audio up or down buttons are pressed
        if (((data[2] & 0x1F) == 0x11 || (data[2] & 0x1F) == 0x12))
        {
            event.event = MIVE_EVENT_VAN_UPDATE_AUDIO_MENU;
        }
        // Change the menu when the audio button is let go. Will probably go tits up when the button is held down for more than 2 secs
        if (data[2] == 0x56)
        {
            event.event = MIVE_EVENT_VAN_NEXT_AUDIO_MENU_ITEM;
        }
    }

    if(event.event)
    {
        xQueueSendToBack(g_global_state.global_main_queue, &event, 0);
    }

    return MIVE_OK;
}

static int psa_parse_rpm_iden(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA
    };
    mive_uart_queue_packet_t* queue_packet;

    if (data_buffers->engine_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    struct psa_engine_data *engine_data = (struct psa_engine_data *)(data_buffers->engine_data);

    struct psa_van_rpm_speed_struct const *const rpm_data = (struct psa_van_rpm_speed_struct *)data;

    engine_data->rpm = be16toh(rpm_data->rpm);
    engine_data->speed = be16toh(rpm_data->speed);

    queue_packet = get_packet_buffer();

    queue_packet->idens[0] = PSA_IDENT_ENGINE;
    queue_packet->num_idens = 1;

    queue_event.ev_data = (void*)queue_packet;

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

static int psa_parse_dash_iden(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    uint32_t mileage;
    union dash_mileage temp_mileage;
    mive_uart_queue_packet_t* queue_packet = NULL;
    
    mive_uart_queue_packet_t queue_packet_c = {
        .idens = {PSA_IDENT_DASHBOARD, PSA_IDENT_ENGINE},
        .num_idens = 2,
    };
    
    
    if (data_buffers->dash_data == NULL || data_buffers->engine_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    queue_packet = get_packet_buffer();
    
    *queue_packet = queue_packet_c;

    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA,
        .ev_data = queue_packet,
    };

    struct psa_dash_data *dash_data = (struct psa_dash_data *)(data_buffers->dash_data);
    struct psa_engine_data *engine_data = (struct psa_engine_data *)(data_buffers->engine_data);
    // struct psa_door_data *door_data = (struct psa_door_data *)(data_buffers->door_data);

    struct psa_van_dashboard *van_data = (struct psa_van_dashboard *)data;

    dash_data->accesories_on = van_data->accesories_on;
    dash_data->backlight_off = van_data->is_backlight_off;
    dash_data->brightness = van_data->brightness;
    dash_data->door_open = van_data->door_open;
    dash_data->economy_mode = van_data->economy_mode;
    dash_data->engine_running = van_data->engine_running;
    dash_data->external_temp = van_data->external_temp;
    dash_data->ignition_on = van_data->ignition_on;

    dash_data->backlight_off = van_data->is_backlight_off;
    dash_data->reverse_gear = van_data->reverse_gear;
    dash_data->water_temp = van_data->water_temp;
    engine_data->engine_temperature = van_data->water_temp;
    engine_data->reverse = van_data->reverse_gear;
    mileage = van_data->mileage;

    temp_mileage.number = van_data->mileage;

    dash_data->mileage = mileage_swap_bytes(temp_mileage);

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

static int psa_parse_trip_iden(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    mive_uart_queue_packet_t* queue_packet = NULL;

    mive_uart_queue_packet_t queue_packet_c = {
        .idens = {PSA_IDENT_DOORS, PSA_IDENT_TRIP},
        .num_idens = 2,
    };

    if (data_buffers->door_data == NULL || data_buffers->trip_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    queue_packet = get_packet_buffer();
    
    *queue_packet = queue_packet_c;

    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA,
        .ev_data = queue_packet,
    };

    struct psa_trip_data *trip_data = (struct psa_trip_data *)(data_buffers->trip_data);
    struct psa_door_data *door_data = (struct psa_door_data *)(data_buffers->door_data);

    struct psa_van_trip_computer *van_data = (struct psa_van_trip_computer *)data;

    door_data->open_doors = (van_data->door_status.packed);

    if (door_data->open_doors)
    {
        door_data->door_open = 1;
    }
    else
    {
        door_data->door_open = 0;
    }

    trip_data->current_fuel_consumption = be16toh(van_data->current_fuel_consumption);
    trip_data->distance_to_empty = be16toh(van_data->distance_to_empty);
    trip_data->trip_1_distance = be16toh(van_data->trip_1_distance);
    trip_data->trip_1_fuel_consumption = be16toh(van_data->trip_1_fuel_consumption);
    trip_data->trip_1_speed = van_data->trip_1_speed;

    trip_data->trip_2_distance = be16toh(van_data->trip_2_distance);
    trip_data->trip_2_fuel_consumption = be16toh(van_data->trip_2_fuel_consumption);
    trip_data->trip_2_speed = van_data->trip_2_speed;
    trip_data->trip_button_pressed = van_data->trip_button.trip_button;

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

// 0x554 len 22
static int psa_parse_radio_tuner_iden(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    mive_uart_queue_packet_t* queue_packet = NULL;

    mive_uart_queue_packet_t queue_packet_c = {
        .idens = {PSA_IDENT_RADIO, PSA_IDENT_RADIO_PRESETS},
        .num_idens = 2,
    };

    if (data_buffers->radio_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    if (data[1] != 0xD1) // D1 is frequency info
    {
        return -MIVE_ERR_VAN_UNKNOWN_IDEN;
    }

    queue_packet = get_packet_buffer();
    
    *queue_packet = queue_packet_c;

    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA,
        .ev_data = queue_packet,
    };

    struct psa_van_radio_freq_info *van_data = (struct psa_van_radio_freq_info *)data;
    struct psa_radio_data *radio_data = (struct psa_radio_data *)data_buffers->radio_data;

    struct psa_preset_data *preset_data = NULL;

    memset(radio_data->station, 0, 9);
    strncpy(radio_data->station, (const char *)van_data->station, 8);

    uint16_t freq = (uint16_t)(van_data->frequency[1]) << 8 | van_data->frequency[0];

    radio_data->freq = freq * 5 + 5000;

    radio_data->signal_strength = van_data->signal_info & 0x0F;
    // radio_data->freq = uint16_t(van_data->frequency[1]) << 8 | van_data->frequency[0]; // swap_endian_uint16(van_data->frequency);

    radio_data->preset = van_data->memory_position;

    switch (van_data->band)
    {
    case PSA_VAN_BAND_FM1:
        radio_data->band = PSA_FM_1;
        preset_data = (struct psa_preset_data *)data_buffers->presets_data_fm_1;
        break;
    case PSA_VAN_BAND_FM2:
        radio_data->band = PSA_FM_2;
        preset_data = (struct psa_preset_data *)data_buffers->presets_data_fm_2;
        break;
    case PSA_VAN_BAND_FM3:
        radio_data->band = PSA_FM_3;
        break;
    case PSA_VAN_BAND_FMAST:
        radio_data->band = PSA_FM_AST;
        preset_data = (struct psa_preset_data *)data_buffers->presets_data_fm_ast;

        break;
    case PSA_VAN_BAND_AM:
        radio_data->band = PSA_AM_1;
        preset_data = (struct psa_preset_data *)data_buffers->presets_data_am;
        break;
    case PSA_VAN_BAND_NONE:
    case PSA_VAN_BAND_PTY_SELECT:
    default:
        radio_data->band = PSA_NONE;
        break;
    }

    if (radio_data->preset > 0 && radio_data->preset <= 6 && preset_data != NULL)
    {
        strncpy(preset_data->presets[radio_data->preset - 1].preset_name, (const char *)van_data->station, 8);
        preset_data->presets->preset_num = radio_data->preset;
    }

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

// 0x554 len 10
static int psa_parse_radio_cd_short_iden(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    mive_uart_queue_packet_t* queue_packet = NULL;
    
    mive_uart_queue_packet_t queue_packet_c = {
        .idens = {PSA_IDENT_CD_PLAYER},
        .num_idens = 1,
    };

    if (data_buffers->cd_player_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    if (data[1] != 0xD6) // D6 is CD info
    {
        return -MIVE_ERR_VAN_UNKNOWN_IDEN;
    }

    queue_packet = get_packet_buffer();
    
    *queue_packet = queue_packet_c;

    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA,
        .ev_data = queue_packet,
    };

    struct psa_van_radio_cd_info_len_10 *van_data = (struct psa_van_radio_cd_info_len_10 *)data;
    struct psa_cd_player_data *cd_data = (struct psa_cd_player_data *)data_buffers->cd_player_data;

    cd_data->current_minute = bcd_to_dec(van_data->minutes);
    cd_data->current_second = bcd_to_dec(van_data->seconds);
    cd_data->current_track = bcd_to_dec(van_data->current_track);
    cd_data->total_tracks = bcd_to_dec(van_data->total_tracks);

    cd_data->status = 0;

    switch (van_data->status)
    {
    case 0x10:
        cd_data->status |= PSA_CD_PLAYER_ERROR;
        break;
    case 0x11:
        cd_data->status |= PSA_CD_PLAYER_LOADING;
        break;
    case 0x12:
        cd_data->status |= PSA_CD_PLAYER_LOADING | PSA_CD_PLAYER_PAUSED;
        break;
    case 0x13:
        cd_data->status |= PSA_CD_PLAYER_LOADING | PSA_CD_PLAYER_PLAYING;
        break;
    case 0x02:
        cd_data->status |= PSA_CD_PLAYER_PAUSED;
        break;
    case 0x03:
        cd_data->status |= PSA_CD_PLAYER_PLAYING;
        break;
    case 0x04:
        cd_data->status |= PSA_CD_PLAYER_FAST_FORWARDING;
        break;
    case 0x05:
        cd_data->status |= PSA_CD_PLAYER_REWINDING;
        break;
    default:
        cd_data->status = 0;
        break;
    }

    cd_data->total_minutes = 0xff;
    cd_data->total_seconds = 0xff;

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

// 0x554 len 19
static int psa_parse_radio_cd_long_iden(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    mive_uart_queue_packet_t* queue_packet = NULL;
    
    mive_uart_queue_packet_t queue_packet_c = {
        .idens = {PSA_IDENT_CD_PLAYER},
        .num_idens = 1,
    };

    if (data_buffers->cd_player_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    if (data[1] != 0xD6) // D6 is CD info
    {
        return -MIVE_ERR_VAN_UNKNOWN_IDEN;
    }

    queue_packet = get_packet_buffer();
    
    *queue_packet = queue_packet_c;

    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA,
        .ev_data = queue_packet,
    };

    struct psa_van_radio_cd_info_len_19 *van_data = (struct psa_van_radio_cd_info_len_19 *)data;
    struct psa_cd_player_data *cd_data = (struct psa_cd_player_data *)data_buffers->cd_player_data;

    cd_data->current_minute = bcd_to_dec(van_data->minutes);
    cd_data->current_second = bcd_to_dec(van_data->seconds);
    cd_data->current_track = bcd_to_dec(van_data->current_track);
    cd_data->total_tracks = bcd_to_dec(van_data->total_tracks);
    cd_data->total_minutes = bcd_to_dec(van_data->total_minutes);
    cd_data->total_seconds = bcd_to_dec(van_data->total_seconds);

    cd_data->status = 0;

    switch (van_data->status)
    {
    case 0x10:
        cd_data->status |= PSA_CD_PLAYER_ERROR;
        break;
    case 0x11:
        cd_data->status |= PSA_CD_PLAYER_LOADING;
        break;
    case 0x12:
        cd_data->status |= PSA_CD_PLAYER_LOADING | PSA_CD_PLAYER_PAUSED;
        break;
    case 0x13:
        cd_data->status |= PSA_CD_PLAYER_LOADING | PSA_CD_PLAYER_PLAYING;
        break;
    case 0x02:
        cd_data->status |= PSA_CD_PLAYER_PAUSED;
        break;
    case 0x03:
        cd_data->status |= PSA_CD_PLAYER_PLAYING;
        break;
    case 0x04:
        cd_data->status |= PSA_CD_PLAYER_FAST_FORWARDING;
        break;
    case 0x05:
        cd_data->status |= PSA_CD_PLAYER_REWINDING;
        break;
    default:
        cd_data->status = 0;
        break;
    }

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

static int psa_parse_radio_preset_iden(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    // if (data_buffers == NULL)
    // {
    //     return -MIVE_ERR_INVALID_ARGUMENT;
    // }

    // if (data_buffers->presets_data == NULL)
    // {
    //     return -MIVE_ERR_INVALID_ARGUMENT;
    // }

    // if (data[1] != 0xD3) // D3 is preset info
    // {
    //     return -MIVE_ERR_VAN_UNKNOWN_IDEN;
    // }

    // struct psa_van_radio_preset_info *van_data = (struct psa_van_radio_preset_info *)data;

    // struct psa_preset_data *preset_data = (struct psa_preset_data *)data_buffers->presets_data;

    // if (van_data->position > 0 && van_data->position < 7)
    // {
    //     memset(preset_data->presets[van_data->position].preset_name, 0, 10);
    //     strncpy(preset_data->presets[van_data->position].preset_name, (const char *)van_data->station_name, 8);

    //     preset_data->presets[van_data->position].preset_num = van_data->position;
    // }
    // else
    // {
    //     return PSA_INVALID_VAN_PACKET;
    // }

    return MIVE_OK;
}

static int psa_parse_headunit_iden(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    mive_uart_queue_packet_t* queue_packet = NULL;
    
    mive_uart_queue_packet_t queue_packet_c = {
        .idens = {PSA_IDENT_HEADUNIT},
        .num_idens = 1,
    };
    
    if (data_buffers->headunit_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    queue_packet = get_packet_buffer();
    
    *queue_packet = queue_packet_c;

    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA,
        .ev_data = queue_packet,
    };

    struct psa_van_radio_settings *van_data = (struct psa_van_radio_settings *)data;

    struct psa_headunit_data *headunit_data = (struct psa_headunit_data *)data_buffers->headunit_data;

    headunit_data->unit_powered_on = van_data->power.power_on;
    headunit_data->auto_volume = van_data->audio_properties.auto_volume;
    headunit_data->balance = ((int8_t)0x3f - van_data->balance.value);
    headunit_data->fader = ((int8_t)0x3f - van_data->fader.value);
    headunit_data->bass = ((int8_t)van_data->bass.value - 0x3f);
    headunit_data->treble = ((int8_t)van_data->treble.value - 0x3f);
    headunit_data->loudness_on = van_data->audio_properties.loudness_on;
    headunit_data->volume = van_data->volume.value;

    headunit_data->muted = van_data->audio_properties.external_mute || van_data->audio_properties.stalk_mute;

    switch (van_data->source.source)
    {
    case 0x01: // Tuner
        headunit_data->source = PSA_RADIO_TUNER;
        break;
    case 0x02: // Tape or CD
        headunit_data->source = PSA_RADIO_INTERNAL;
        break;
    case 0x03: // Cd changer
        headunit_data->source = PSA_RADIO_EXTERNAL;
        break;
    case 0x05: // Navigation
        headunit_data->source = PSA_RADIO_NAVIGATION;
        break;
    default:
        headunit_data->source = PSA_RADIO_NONE;
        break;
    }

    headunit_data->cd_present = van_data->source.cd_present;
    headunit_data->tape_present = van_data->source.tape_present;
    // headunit_data->settings_menu_open = van_data->audio_properties.audio_properties_menu_open;

    // headunit_data->settings_changing.balance_changing = van_data->balance.updating;
    // headunit_data->settings_changing.bass_changing = van_data->bass.updating;
    // headunit_data->settings_changing.fader_changing = van_data->fader.updating;
    // headunit_data->settings_changing.treble_changing = van_data->treble.updating;
    // headunit_data->settings_changing.volume_changing = van_data->volume.updating;

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

static int psa_parse_vin_iden(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    mive_uart_queue_packet_t* queue_packet = NULL;

    mive_uart_queue_packet_t queue_packet_c = {
        .idens = {PSA_IDENT_VIN},
        .num_idens = 1,
    };

    if (data_buffers->vin_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    queue_packet = get_packet_buffer();
    
    *queue_packet = queue_packet_c;

    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA,
        .ev_data = queue_packet,
    };
    // char *vin = (char *)data_buffers->vin_data;
    int const vin_size = 17;

    memcpy(data_buffers->vin_data, data, vin_size);

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

// 0x4FC len 11
static int psa_parse_instruments_short_iden(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    mive_uart_queue_packet_t* queue_packet = NULL;
    mive_uart_queue_packet_t queue_packet_c = {
        .idens = {PSA_IDENT_ENGINE},
        .num_idens = 1,
    };

    if (data_buffers->engine_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    queue_packet = get_packet_buffer();
    
    *queue_packet = queue_packet_c;

    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA,
        .ev_data = queue_packet,
    };

    struct psa_engine_data *engine_data = (struct psa_engine_data *)data_buffers->engine_data;
    struct psa_van_instrument_cluster_short *van_data = (struct psa_van_instrument_cluster_short *)data;

    engine_data->fuel_level = van_data->fuel_level;
    engine_data->oil_temperature = van_data->oil_temp;

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

// 0x4FC len 14
static int psa_parse_instruments_long_iden(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    mive_uart_queue_packet_t* queue_packet = NULL;
    mive_uart_queue_packet_t queue_packet_c = {
        .idens = {PSA_IDENT_ENGINE},
        .num_idens = 1,
    };

    if (data_buffers->engine_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    queue_packet = get_packet_buffer();
    
    *queue_packet = queue_packet_c;

    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA,
        .ev_data = queue_packet,
    };

    struct psa_engine_data *engine_data = (struct psa_engine_data *)data_buffers->engine_data;
    struct psa_van_instrument_cluster_long *van_data = (struct psa_van_instrument_cluster_long *)data;

    engine_data->fuel_level = van_data->fuel_level;
    engine_data->oil_temperature = van_data->oil_temp;

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

// 0x524 len 14
static int psa_parse_car_status_2_short(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    mive_uart_queue_packet_t* queue_packet = NULL;
    mive_uart_queue_packet_t queue_packet_c = {
        .idens = {PSA_IDENT_CAR_STATUS},
        .num_idens = 1,
    };

    if (data_buffers->status_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    queue_packet = get_packet_buffer();
    
    *queue_packet = queue_packet_c;

    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA,
        .ev_data = queue_packet,
    };


    struct psa_status_data *status_data = (struct psa_status_data *)data_buffers->status_data;
    struct psa_van_car_status_2_short *van_data = (struct psa_van_car_status_2_short *)data;

    status_data->doors_locked = van_data->Field8.doors_locked;
    status_data->deadlocking_active = van_data->Field8.deadlocking_active;

    status_data->key_in_ignition = van_data->Field5.ignition_key_left_in;

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

// 0x524 len 16
static int psa_parse_car_status_2_long(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    mive_uart_queue_packet_t* queue_packet = NULL;
    mive_uart_queue_packet_t queue_packet_c = {
        .idens = {PSA_IDENT_CAR_STATUS},
        .num_idens = 1,
    };

    if (data_buffers->status_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    queue_packet = get_packet_buffer();
    
    *queue_packet = queue_packet_c;

    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA,
        .ev_data = queue_packet,
    };

    struct psa_status_data *status_data = (struct psa_status_data *)data_buffers->status_data;
    struct psa_van_car_status_2_long *van_data = (struct psa_van_car_status_2_long *)data;

    status_data->doors_locked = van_data->Field8.doors_locked;
    status_data->deadlocking_active = van_data->Field8.deadlocking_active;

    status_data->key_in_ignition = van_data->Field5.ignition_key_left_in;

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

static int psa_parse_cdc_command(
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    mive_uart_queue_packet_t* queue_packet = NULL;
    mive_uart_queue_packet_t queue_packet_c = {
        .idens = {PSA_IDENT_CAR_STATUS},
        .num_idens = 1,
    };

    if (data_buffers->status_data == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    queue_packet = get_packet_buffer();
    
    *queue_packet = queue_packet_c;

    struct mive_global_event queue_event = {
        .event = MIVE_EVENT_UART_NEW_DATA,
        .ev_data = queue_packet,
    };

    struct psa_status_data *status_data = (struct psa_status_data *)data_buffers->status_data;
    struct psa_van_cd_changer_command_struct *van_data = (struct psa_van_cd_changer_command_struct *)data;

    uint16_t command = be16toh(van_data->command);

    switch (command)
    {
    case PSA_VAN_CD_CHANGER_PLAY:
        status_data->cd_changer_command = PSA_CD_CHANGER_COMM_PLAY;
        break;
    case PSA_VAN_CD_CHANGER_PAUSE:
        status_data->cd_changer_command = PSA_CD_CHANGER_COMM_PAUSE;
        break;
    case PSA_VAN_CD_CHANGER_NEXT_TRACK:
        status_data->cd_changer_command = PSA_CD_CHANGER_COMM_NEXT_TRACK;
        break;
    case PSA_VAN_CD_CHANGER_PREVIOUS_TRACK:
        status_data->cd_changer_command = PSA_CD_CHANGER_COMM_PREV_TRACK;
        break;
    default:
        status_data->cd_changer_command = PSA_CD_CHANGER_COMM_NONE;
        break;
    }

    xQueueSendToBack(g_global_state.global_main_queue, &queue_event, 0);

    return MIVE_OK;
}

int psa_parse_van_packet(
    uint16_t iden,
    uint8_t size,
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers)
{
    if (data == NULL || data_buffers == NULL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    switch (iden)
    {
    case PSA_VAN_IDEN_RPM:
        if (size != sizeof(struct psa_van_rpm_speed_struct))
        {
            return -MIVE_ERR_VAN_INVALID_PACKET_SIZE;
        }

        return psa_parse_rpm_iden(data, (struct psa_output_data_buffers *)data_buffers);
        break;

    case PSA_VAN_IDEN_CAR_STATUS1:
        if (size != sizeof(struct psa_van_trip_computer))
        {
            return -MIVE_ERR_VAN_INVALID_PACKET_SIZE;
        }

        return psa_parse_trip_iden(data, (struct psa_output_data_buffers *)data_buffers);
        break;

    case PSA_VAN_IDEN_ENGINE:
        if (size != sizeof(struct psa_van_dashboard))
        {
            return -MIVE_ERR_VAN_INVALID_PACKET_SIZE;
        }

        return psa_parse_dash_iden(data, (struct psa_output_data_buffers *)data_buffers);
        break;
    case PSA_VAN_IDEN_VIN:
        if (size != 17)
        {
            return -MIVE_ERR_VAN_INVALID_PACKET_SIZE;
        }

        return psa_parse_vin_iden(data, (struct psa_output_data_buffers *)data_buffers);

        break;

    case PSA_VAN_IDEN_HEAD_UNIT:
        if (size == 22)
        {
            return psa_parse_radio_tuner_iden(data, (struct psa_output_data_buffers *)data_buffers);
        }
        else if (size == 10)
        {
            return psa_parse_radio_cd_short_iden(data, (struct psa_output_data_buffers *)data_buffers);
        }
        else if (size == 19)
        {
            return psa_parse_radio_cd_long_iden(data, (struct psa_output_data_buffers *)data_buffers);
        }
        // else if (size == 12)
        // {
        //     return psa_parse_radio_preset_iden(data, (struct psa_output_data_buffers *)data_buffers);
        // }
        else
        {
            return -MIVE_ERR_VAN_INVALID_PACKET_SIZE;
        }

        break;
    case PSA_VAN_IDEN_AUDIO_SETTINGS:
        if (size != 11)
        {
            return -MIVE_ERR_VAN_INVALID_PACKET_SIZE;
        }

        return psa_parse_headunit_iden(data, (struct psa_output_data_buffers *)data_buffers);
        break;

    case PSA_VAN_IDEN_LIGHTS_STATUS:
        if (size == 11)
        {
            return psa_parse_instruments_short_iden(data, (struct psa_output_data_buffers *)data_buffers);
        }
        else if (size == 14)
        {
            return psa_parse_instruments_long_iden(data, (struct psa_output_data_buffers *)data_buffers);
        }
        else
        {
            return -MIVE_ERR_VAN_INVALID_PACKET_SIZE;
        }
        break;

    case PSA_VAN_IDEN_CAR_STATUS2:
        if (size == 14)
        {
            return psa_parse_car_status_2_short(data, (struct psa_output_data_buffers *)data_buffers);
        }
        else if (size == 16)
        {
            return psa_parse_car_status_2_long(data, (struct psa_output_data_buffers *)data_buffers);
        }
        else
        {
            return -MIVE_ERR_VAN_INVALID_PACKET_SIZE;
        }

    case PSA_VAN_IDEN_CDCHANGER_COMMAND:
        if (size == 2)
        {
            return psa_parse_cdc_command(data, (struct psa_output_data_buffers *)data_buffers);
        }
        else
        {
            return -MIVE_ERR_VAN_INVALID_PACKET_SIZE;
        }

    case PSA_VAN_IDEN_DEVICE_REPORT:
        if (size == 3)
        {
            return psa_parse_buttons_iden(data, (struct psa_output_data_buffers *)data_buffers);
        }
        else
        {
            return -MIVE_ERR_VAN_INVALID_PACKET_SIZE;
        }

    default:
        return -MIVE_ERR_VAN_UNKNOWN_IDEN;
        break;
    }

    return MIVE_OK;
}
