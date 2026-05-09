#ifndef _PSA_HELPER_H
#define _PSA_HELPER_H

#include <stdint.h>
#include "include/global_tasks.h"
#include "include/van/van_packet_parser.h"

#define PSA_MAIN_UART_SEND_BUFFERS_NUM 20
#define PSA_MAIN_TSS_BUFFERS_NUM 20

enum psa_radio_preset_band
{
    PSA_PRESET_AM = 0,
    PSA_PRESET_FM_1 = 1,
    PSA_PRESET_FM_2 = 2,
    PSA_PRESET_FMAST = 3,
    PSA_PRESET_MAX = 4,
};

extern struct psa_output_data_buffers global_libpsa_buffers;

extern mive_uart_task_packet_t* global_uart_send_buffers;
extern mive_uart_task_packet_t* global_uart_receive_buffers;
extern mive_tss_task_packet_t* global_tss_buffers;

extern void libpsa_send_packet(uint16_t ident);
extern void init_libpsa_packets(void);
extern void libpsa_update_audio_settings_packet(int audio_menu_open, int audio_menu_setting);

extern mive_tss_task_packet_t* get_tss_task_buffer(void);

extern void rd3_send_command_packet(uint8_t* packet_data, uint8_t const packet_size);
extern void rd3_switch_source(enum psa_radio_source source);
extern void rd3_send_state_change(void);
extern void rd3_send_audio_settings(void);
extern void emf_send_reply_request(uint16_t iden, uint8_t size);
extern void emf_receive(uint16_t iden, uint8_t size);
extern void emf_audio_setting_update(int setting_num, int update_value_by);
#endif // _PSA_HELPER_H