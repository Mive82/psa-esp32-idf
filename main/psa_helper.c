#include "freertos/FreeRTOS.h"

#include <stdint.h>
#include <string.h>

#include "include/mive/psa_helper.h"
#include "include/van/van_packet_parser.h"
#include "include/global_tasks.h"
#include "include/global_state_def.h"

static int uart_send_buffer_num = 0;

static mive_uart_task_packet_t* get_uart_send_buffer(void)
{
    mive_uart_task_packet_t* to_ret = &global_uart_send_buffers[uart_send_buffer_num];

    uart_send_buffer_num = (uart_send_buffer_num >= (PSA_MAIN_UART_SEND_BUFFERS_NUM - 1)) ? 0 : uart_send_buffer_num + 1;

    return to_ret;
}

void libpsa_send_packet(uint16_t ident)
{
    mive_uart_task_packet_t* packet = get_uart_send_buffer();
    struct mive_global_event global_event = {
        .event = MIVE_EVENT_UART_SEND,
        .ev_data = packet,
    };

    switch (ident)
    {
    case PSA_IDENT_ENGINE:
        packet->data_size = sizeof(*global_libpsa_buffers.engine_data);
        memcpy(
            packet->data, 
            global_libpsa_buffers.engine_data, 
            packet->data_size);
        packet->iden = ident;
        break;
    case PSA_IDENT_RADIO:
        packet->data_size = sizeof(*global_libpsa_buffers.radio_data);
        memcpy(
            packet->data, 
            global_libpsa_buffers.radio_data, 
            packet->data_size);
        packet->iden = ident;
        break;
    case PSA_IDENT_VIN:
        packet->data_size = 17;
        memcpy(
            packet->data, 
            global_libpsa_buffers.vin_data, 
            packet->data_size);
        packet->iden = ident;
        break;
    case PSA_IDENT_RADIO_PRESETS:
        packet->data_size = sizeof(*global_libpsa_buffers.presets_data_am);
        switch(global_libpsa_buffers.radio_data->band)
        {
            case PSA_AM_1:
                memcpy(
                    packet->data, 
                    global_libpsa_buffers.presets_data_am, 
                    packet->data_size);
                break;
            case PSA_FM_1:
                memcpy(
                    packet->data, 
                    global_libpsa_buffers.presets_data_fm_1, 
                    packet->data_size);
                break;
            case PSA_FM_2:
                memcpy(
                    packet->data, 
                    global_libpsa_buffers.presets_data_fm_2, 
                    packet->data_size);
                break;
            case PSA_FM_AST:
                memcpy(
                    packet->data, 
                    global_libpsa_buffers.presets_data_fm_ast, 
                    packet->data_size);
                break;
            default:
                memset(packet->data, 0, packet->data_size);
                break;
        }
        packet->iden = ident;
        break;
    case PSA_IDENT_CAR_STATUS:
        packet->data_size = sizeof(*global_libpsa_buffers.status_data);
        memcpy(
            packet->data, 
            global_libpsa_buffers.status_data, 
            packet->data_size);
        packet->iden = ident;
        global_libpsa_buffers.status_data->cd_changer_command = PSA_CD_CHANGER_COMM_NONE;
        break;
    case PSA_IDENT_HEADUNIT:
        packet->data_size = sizeof(*global_libpsa_buffers.headunit_data);
        memcpy(
            packet->data,
            global_libpsa_buffers.headunit_data,
            packet->data_size
        );
        packet->iden = ident;
        break;
    case PSA_IDENT_CD_PLAYER:
        packet->data_size = sizeof(*global_libpsa_buffers.cd_player_data);
        memcpy(
            packet->data,
            global_libpsa_buffers.cd_player_data,
            packet->data_size
        );
        packet->iden = ident;
        break;
    case PSA_IDENT_DASHBOARD:
        packet->data_size = sizeof(*global_libpsa_buffers.dash_data);
        memcpy(
            packet->data,
            global_libpsa_buffers.dash_data,
            packet->data_size
        );
        packet->iden = ident;
        break;
    case PSA_IDENT_TRIP:
        packet->data_size = sizeof(*global_libpsa_buffers.trip_data);
        memcpy(
            packet->data,
            global_libpsa_buffers.trip_data,
            packet->data_size    
        );
        packet->iden = ident;
        break;
    case PSA_IDENT_DOORS:
        packet->data_size = sizeof(*global_libpsa_buffers.door_data);
        memcpy(
            packet->data,
            global_libpsa_buffers.door_data,
            packet->data_size
        );
        packet->iden = ident;
        break;
    default:
        packet->iden = 0;
        packet->data_size = 0;
        break;
    }

    xQueueSendToBack(g_global_state.global_uart_queue, &global_event, 0);
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

    global_uart_send_buffers = calloc(PSA_MAIN_UART_SEND_BUFFERS_NUM, sizeof(*global_uart_send_buffers));
    global_uart_receive_buffers = calloc(PSA_MAIN_UART_SEND_BUFFERS_NUM, sizeof(*global_uart_send_buffers));
}

void libpsa_update_audio_settings_packet(
    int audio_menu_open,
    int audio_menu_setting)
{
    struct psa_headunit_data *audio_settings_output_buffer = global_libpsa_buffers.headunit_data;
    audio_settings_output_buffer->settings_menu_open = audio_menu_open;
    switch (audio_menu_setting)
    {
    case 0:

        audio_settings_output_buffer->settings_changing.auto_volume_changing = 0;
        audio_settings_output_buffer->settings_changing.balance_changing = 0;
        audio_settings_output_buffer->settings_changing.bass_changing = 0;
        audio_settings_output_buffer->settings_changing.fader_changing = 0;
        audio_settings_output_buffer->settings_changing.loudness_changing = 0;
        audio_settings_output_buffer->settings_changing.treble_changing = 0;
        audio_settings_output_buffer->settings_changing.volume_changing = 0;

        break;
    case 1: // Bass
        audio_settings_output_buffer->settings_changing.auto_volume_changing = 0;
        audio_settings_output_buffer->settings_changing.balance_changing = 0;
        audio_settings_output_buffer->settings_changing.bass_changing = 1;
        audio_settings_output_buffer->settings_changing.fader_changing = 0;
        audio_settings_output_buffer->settings_changing.loudness_changing = 0;
        audio_settings_output_buffer->settings_changing.treble_changing = 0;
        audio_settings_output_buffer->settings_changing.volume_changing = 0;
        break;

    case 2: // Treble
        audio_settings_output_buffer->settings_changing.auto_volume_changing = 0;
        audio_settings_output_buffer->settings_changing.balance_changing = 0;
        audio_settings_output_buffer->settings_changing.bass_changing = 0;
        audio_settings_output_buffer->settings_changing.fader_changing = 0;
        audio_settings_output_buffer->settings_changing.loudness_changing = 0;
        audio_settings_output_buffer->settings_changing.treble_changing = 1;
        audio_settings_output_buffer->settings_changing.volume_changing = 1;
        break;

    case 3: // Loudness
        audio_settings_output_buffer->settings_changing.auto_volume_changing = 0;
        audio_settings_output_buffer->settings_changing.balance_changing = 0;
        audio_settings_output_buffer->settings_changing.bass_changing = 0;
        audio_settings_output_buffer->settings_changing.fader_changing = 0;
        audio_settings_output_buffer->settings_changing.loudness_changing = 1;
        audio_settings_output_buffer->settings_changing.treble_changing = 0;
        audio_settings_output_buffer->settings_changing.volume_changing = 0;
        break;

    case 4: // Fader
        audio_settings_output_buffer->settings_changing.auto_volume_changing = 0;
        audio_settings_output_buffer->settings_changing.balance_changing = 0;
        audio_settings_output_buffer->settings_changing.bass_changing = 0;
        audio_settings_output_buffer->settings_changing.fader_changing = 1;
        audio_settings_output_buffer->settings_changing.loudness_changing = 0;
        audio_settings_output_buffer->settings_changing.treble_changing = 0;
        audio_settings_output_buffer->settings_changing.volume_changing = 0;
        break;
    case 5: // Balance

        audio_settings_output_buffer->settings_changing.auto_volume_changing = 0;
        audio_settings_output_buffer->settings_changing.balance_changing = 1;
        audio_settings_output_buffer->settings_changing.bass_changing = 0;
        audio_settings_output_buffer->settings_changing.fader_changing = 0;
        audio_settings_output_buffer->settings_changing.loudness_changing = 0;
        audio_settings_output_buffer->settings_changing.treble_changing = 0;
        audio_settings_output_buffer->settings_changing.volume_changing = 0;
        break;
    case 6: // Auto-volume
        audio_settings_output_buffer->settings_changing.auto_volume_changing = 1;
        audio_settings_output_buffer->settings_changing.balance_changing = 0;
        audio_settings_output_buffer->settings_changing.bass_changing = 0;
        audio_settings_output_buffer->settings_changing.fader_changing = 0;
        audio_settings_output_buffer->settings_changing.loudness_changing = 0;
        audio_settings_output_buffer->settings_changing.treble_changing = 0;
        audio_settings_output_buffer->settings_changing.volume_changing = 0;
        break;
    default:
        break;
    }
}
