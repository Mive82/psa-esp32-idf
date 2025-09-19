#ifndef _PSA_HELPER_H
#define _PSA_HELPER_H

#include <stdint.h>
#include "include/global_tasks.h"
#include "include/van/van_packet_parser.h"

#define PSA_MAIN_UART_SEND_BUFFERS_NUM 20

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

extern void libpsa_send_packet(uint16_t ident);
extern void init_libpsa_packets(void);
extern void libpsa_update_audio_settings_packet(int audio_menu_open, int audio_menu_setting);

#endif // _PSA_HELPER_H