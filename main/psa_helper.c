#include "freertos/FreeRTOS.h"

#include <stdint.h>
#include <string.h>

#include "include/mive/psa_helper.h"
#include "include/van/van_packet_parser.h"
#include "include/global_tasks.h"
#include "include/global_state_def.h"

#include "include/van_structs/van_rd3_command_struct.h"

#include "esp_log.h"

static const char* const TAG = "PSAHELPER";

static int uart_send_buffer_num = 0;

static int old_accessory = 0;

static struct psa_van_rd3_command_update_state rd3_state_cache = {0};

static mive_uart_task_packet_t* get_uart_send_buffer(void)
{
    mive_uart_task_packet_t* to_ret = &global_uart_send_buffers[uart_send_buffer_num];

    uart_send_buffer_num = (uart_send_buffer_num + 1) % PSA_MAIN_UART_SEND_BUFFERS_NUM;
    //    uart_send_buffer_num = (uart_send_buffer_num >= (PSA_MAIN_UART_SEND_BUFFERS_NUM - 1)) ? 0 : uart_send_buffer_num + 1;

    return to_ret;
}

void libpsa_send_packet(uint16_t ident)
{
    mive_uart_task_packet_t* packet = get_uart_send_buffer();
    struct mive_global_event global_event = {
        .event = MIVE_EVENT_UART_SEND,
        .ev_data.uart_task_data = packet,
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

void emf_audio_setting_update(int setting_num, int update_value_by)
{
    switch (setting_num)
    {
    case 1: // Bass
        g_radio_state.radio_setting_bass += update_value_by;
        rd3_send_audio_settings();
        break;
    case 2: // Treble
        g_radio_state.radio_setting_treble += update_value_by;
        rd3_send_audio_settings();
        break;
    case 3: // Loudness
        {
            if(update_value_by > 0)
            {
                g_radio_state.radio_setting_loudness = 1;
            }
            else{
                g_radio_state.radio_setting_loudness = 0;
            }
            // This setting is updated differently
            rd3_send_state_change();
        }
        break;
    case 4: // Fader
        g_radio_state.radio_setting_fader += update_value_by;
        rd3_send_audio_settings();
        break;
    case 5: // Balance
        g_radio_state.radio_setting_balance += update_value_by;
        rd3_send_audio_settings();
        break;
    case 6: // Auto-volume
        {
            if(update_value_by > 0)
            {
                g_radio_state.radio_setting_auto_vol = 1;
            }
            else{
                g_radio_state.radio_setting_auto_vol = 0;
            }
            // This setting is updated differently
            rd3_send_state_change();
        }
        break;
    default:
        break;
    }
}

void emf_send_reply_request(uint16_t iden, uint8_t size)
{
    mive_tss_task_packet_t* tss_packet = NULL;
    struct mive_global_event event = {0};

    tss_packet = get_tss_task_buffer();
    tss_packet->iden = iden;
    tss_packet->packet_size = size;
    tss_packet->message_type = TSS_REPLY_REQUEST;

    event.ev_data.tss_event_data = tss_packet;
    event.event = MIVE_EVENT_TSS_WRITE_FRAME;

    xQueueSendToBack(g_global_state.global_tss_queue, &event, 0);
}

void emf_receive(uint16_t iden, uint8_t size)
{
    mive_tss_task_packet_t* tss_packet = NULL;
    struct mive_global_event event = {0};

    tss_packet = get_tss_task_buffer();
    tss_packet->iden = iden;
    tss_packet->packet_size = size;
    tss_packet->message_type = TSS_RECEIVE;

    event.ev_data.tss_event_data = tss_packet;
    event.event = MIVE_EVENT_TSS_WRITE_FRAME;

    xQueueSendToBack(g_global_state.global_tss_queue, &event, 0);
}

void rd3_send_command_packet(uint8_t* packet_data, uint8_t const packet_size)
{
    mive_tss_task_packet_t* tss_packet = NULL;
    struct mive_global_event event = {0};

    if(packet_size > 6)
    {
        // Invalid size
        ESP_LOGE(TAG, "Invalid packet size %d", packet_size);
        return;
    }

    tss_packet = get_tss_task_buffer();
    tss_packet->iden = 0x8d4;
    tss_packet->packet_size = packet_size;
    tss_packet->message_type = TSS_TRANSMIT;
    memcpy(tss_packet->packet, packet_data, packet_size);

    event.ev_data.tss_event_data = tss_packet;
    event.event = MIVE_EVENT_TSS_WRITE_FRAME;

    xQueueSendToBack(g_global_state.global_tss_queue, &event, 0);
}

void rd3_switch_source(
    enum psa_radio_source source)
{
    uint8_t source_regval = 0;
    uint8_t cmd_buff[] = {0x12, 0x00};

    if(g_radio_state.radio_state_current)
    {
        switch (source)
        {
        case PSA_RADIO_TUNER:
            source_regval = 0x01;
            g_radio_state.keyboard_override = (g_radio_state.radio_menu_state == 1);
            break;
        case PSA_RADIO_INTERNAL:
            source_regval = 0x02;
            g_radio_state.keyboard_override = (g_radio_state.radio_menu_state == 1);
            break;
        case PSA_RADIO_EXTERNAL:
            source_regval = 0x03;
            g_radio_state.keyboard_override = 1;
            break;
        case PSA_RADIO_NAVIGATION:
            // source_regval = 0x05; // Actual value
            source_regval = 0x00; // Not supported
            break;
        default:
            break;
        }
    }

    if(source_regval != 0x00)
    {
        cmd_buff[1] = source_regval;
        rd3_send_command_packet(cmd_buff, sizeof(cmd_buff) / sizeof(*cmd_buff));
        rd3_send_state_change();
    }

}

void rd3_send_state_change()
{
    struct psa_van_rd3_command_update_state cmd = {0};

    cmd.command_type = 0x11;

    // If we are in economy mode, always send power off command
    if(!g_radio_state.economy_mode)
    {
        // Update the radio state only when the accessory changes
        if(g_radio_state.accessory != old_accessory)
        {
            if(g_radio_state.accessory)
            {
                cmd.data.data.power = g_radio_state.radio_state_ignition;
            }
            else{
                cmd.data.data.power = 0;
                g_radio_state.radio_state_target = 0;
            }

            old_accessory = g_radio_state.accessory;
        }
        // Else update with custom power state
        else{
            cmd.data.data.power = g_radio_state.radio_state_target;
        }

        cmd.data.data.keyboard_override = (g_radio_state.keyboard_override && g_radio_state.radio_state_current);
        cmd.data.data.auto_volume = (g_radio_state.radio_setting_auto_vol && g_radio_state.radio_state_current);
        cmd.data.data.loudness = (g_radio_state.radio_setting_loudness && g_radio_state.radio_state_current);
        cmd.data.data.key = cmd.data.data.power;
    }

    rd3_send_command_packet((uint8_t*)(&cmd), sizeof(cmd));
}

void rd3_send_audio_settings()
{
    struct psa_van_rd3_command_change_audio_settings cmd = {0};

    // Don't do anything in economy mode
    if(g_radio_state.economy_mode)
    {
        return;
    }

    cmd.command_type = 0x14;

    // If the audio menu is closing, the "updating" bit of balance is set

    if(!g_radio_state.radio_menu_state)
    {
        cmd.balance.updating = 1;
    }

    cmd.balance.value = ((uint8_t)(-g_radio_state.radio_setting_balance + 0x3f)) & 0x7f;
    cmd.fader.value = ((uint8_t)(-g_radio_state.radio_setting_fader + 0x3f)) & 0x7f;
    
    cmd.bass.value = ((uint8_t)(g_radio_state.radio_setting_bass + 0x3f)) & 0x7f;
    cmd.treble.value = ((uint8_t)(g_radio_state.radio_setting_treble + 0x3f)) & 0x7f;

    rd3_send_command_packet((uint8_t*)(&cmd), sizeof(cmd));
}
