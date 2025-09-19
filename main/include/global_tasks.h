#ifndef MIVE_GLOBAL_TASKS_H
#define MIVE_GLOBAL_TASKS_H

typedef struct mive_global_state_t mive_global_state_t;

enum mive_global_event_t
{
    MIVE_EVENT_NONE = 0,
    MIVE_EVENT_GLOBAL_SLEEP,
    MIVE_EVENT_RMT_NEW_VAN_FRAME,
    MIVE_EVENT_TSS_RESET,
    MIVE_EVENT_TSS_ACTIVATE,
    MIVE_EVENT_TSS_IDLE,
    MIVE_EVENT_TSS_SLEEP,
    MIVE_EVENT_TSS_WRITE_FRAME,
    MIVE_EVENT_TSS_MONITOR_CHANNEL,

    MIVE_EVENT_VAN_UPDATE_AUDIO_MENU,
    MIVE_EVENT_VAN_NEXT_AUDIO_MENU_ITEM,
    MIVE_EVENT_VAN_CLOSE_AUDIO_MENU,

    MIVE_EVENT_UART_NEW_DATA, // New VAN Data to send
    MIVE_EVENT_UART_RECEIVE, // Received data from UART
    MIVE_EVENT_UART_SEND, // Send data to UART

    MIVE_EVENT_TIMER_UART_SEND,
    MIVE_EVENT_TIMER_1MS,
    MIVE_EVENT_TIMER_1S,

    MIVE_EVENT_ADC,
    MIVE_EVENT_STATE_CHANGE,
    MIVE_EVENT_POWER,
};

struct mive_global_event
{
    enum mive_global_event_t event;
    void* ev_data;
};

/* VAN Task type definitions */

#define VAN_MAX_PACKET_LEN 40

// ev_data for MIVE_EVENT_RMT_NEW_VAN_FRAME
typedef struct mive_van_packet
{
    uint16_t iden;
    uint8_t packet_size;
    uint8_t* packet;

} mive_van_packet_t;

/* TSS Task type definitions */

typedef struct mive_tss_task_packet
{
    uint16_t iden;
    uint8_t message_type; // enum tss_message_type in tss463.h
    uint8_t message_channel;
    uint8_t packet[VAN_MAX_PACKET_LEN];
    uint8_t packet_size;
} mive_tss_task_packet_t;

/* 
UART Task event data
    Main -> UART task = Send packet
    UART task -> Main = Received packet
*/
typedef struct mive_uart_task_packet
{
    uint16_t iden; // uart message identifier
    uint8_t data_size; // Size of the data
    uint8_t data[128]; // Data portion of the packet
} mive_uart_task_packet_t;

/*
UART Queue event data.
New VAN data parsed -> queue uart send

*/
typedef struct mive_uart_queue_packet
{
    uint16_t num_idens; // Number of idens to send
    uint16_t idens[10]; // uart message identifiers
} mive_uart_queue_packet_t;

/* Task function definitions */

void van_rmt_task(void* params);
void tss_task(void* params);
void uart_task(void* params);

#endif // MIVE_GLOBAL_TASKS_H