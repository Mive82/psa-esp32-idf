#ifndef TSS463C_H
#define TSS463C_H

#include <stdint.h>
#include "driver/spi_master.h"
#include "freertos/semphr.h"

#define TSS_CHANNEL_ADDR(x) (0x10 + (0x08 * x))
#define TSS_CHANNEL_MESS_PTR(x) TSS_CHANNEL_ADDR(x) + 2
#define TSS_CHANNEL_MESS_LEN_AND_STATUS(x) TSS_CHANNEL_ADDR(x) + 3
#define TSS_MAILBOX_OFFSET 0x80
#define TSS_GETMAIL(x) (TSS_MAILBOX_OFFSET + x)

#define TSS_8MHz_62k5BPS 0x30
#define TSS_8MHz_125kBPS 0x20

#define TSS_SELECT() gpio_set_level(instance->cs_pin, 0)
// #define TSS_SELECT() 
#define TSS_DESELECT() gpio_set_level(instance->cs_pin, 1)
// #define TSS_DESELECT()


#define TSS_WRITE 0xE0
#define TSS_READ 0x60

#define TSS_MAX_CHANNEL 14
#define TSS_MAX_MEMORY_ADDR 0x7f

enum tss_message_type {
    TSS_NONE = 0,
    TSS_TRANSMIT,
    TSS_TRANSMIT_NOACK,
    TSS_REPLY_REQUEST,
    TSS_IMM_REPLY,
    TSS_DEF_REPLY,
};

enum tss_chip_mode {
    TSS_MODE_IDLE = 0,
    TSS_MODE_ACTIVE,
    TSS_MODE_SLEEP,
};

struct tss_channel_t
{
    uint16_t identifier; // Ident registered to the channel
    uint8_t ram_address; // The RAM address in use
    uint8_t data_size;   // Memory size allocated
    uint8_t message_type; // Message type, see enum tss_message_type
    uint8_t is_occupied; // Is the channel registered?
    uint8_t is_busy;     // Is the channel doing something?
    uint8_t tx_attempt;  // Number of attempts to transmit said channel (starts at 1)
    uint8_t message_len_and_status_reg_value;
};

typedef struct tss_channel_t tss_channel_t;

typedef struct
{
    uint8_t start;
    uint8_t end;
} tss_ringbuffer_t;

struct tss_instance_t
{
    spi_device_handle_t tss_handle;
    uint8_t *tx_buffer;
    uint8_t *rx_buffer;
    uint32_t buffers_size;
    uint8_t cs_pin;
    uint8_t interrupt_pin;
    enum tss_chip_mode chip_mode;

    SemaphoreHandle_t tss_spi_semaphore;
    tss_ringbuffer_t tss_ringbuffer; 
    tss_channel_t channels[TSS_MAX_CHANNEL];
};

typedef struct tss_instance_t tss_instance_t;

typedef enum 
{
    TSS_EVENT_TRANSMIT_CHANNEL = 0, // Queue specified channel for transmission (first time)
    TSS_EVENT_CHECK_CHANNEL, // Check on the channel's status
    TSS_EVENT_RETRANSMIT_CHANNEL, // Retransmit specified channel
    TSS_EVENT_CLEAN_CHANNEL, // Cleanup channel after transmission
    TSS_EVENT_CANCEL_CHANNEL, // Abort sending channel

    TSS_EVENT_MAX,
} tss_event_t;

typedef struct
{
    tss_event_t event_type;
    int channel_num;
} tss_event_command;


extern tss_instance_t global_tss_instance;

/* ================================================================ */

int tss_register_get(
    tss_instance_t *const instance,
    uint8_t const reg_addr,
    uint8_t *const reg_value);

int tss_register_set(
    tss_instance_t *instance,
    uint8_t reg_addr,
    uint8_t reg_value);

int tss_registers_set(
    tss_instance_t *const instance,
    uint8_t const reg_addr,
    uint8_t const *const values,
    uint8_t const count);

int tss_registers_get(
    tss_instance_t *const instance,
    uint8_t const reg_addr,
    uint8_t *const buffer,
    uint8_t count);

/* ================================================================ */

/**
 * @brief Get the next available mailbox address to use for channel
 * 
 * 
 * @param instance 
 * @param buf_size 
 * @param addr 
 * @return MIVE_OK or MIVE_ERR_OUTOFMEMORY or MIVE_ERR_INVALID_ARGUMENT
 */
int tss_get_memory_address(
    tss_instance_t* const instance,
    uint8_t const channel_num,
    uint8_t const buf_size,
    uint8_t *const addr);

/* ================================================================ */

int tss_abort_channel(
    tss_instance_t* const instance,
    uint8_t const channel_num);

int tss_free_channel(
    tss_instance_t* const instance,
    uint8_t const channel_num);

int tss_retransmit_channel(
    tss_instance_t* const instance,
    uint8_t const channel_num);

/* ================================================================ */
/* Convinience functions to get certain register values */

uint8_t tss_get_channel_status_register(
    tss_instance_t* const instance,
    uint8_t const channel_num);

/* ================================================================ */
/* Only transmissions are handled by the TSS, the receiving is done */
/* by the RMT peripheral                                            */

int tss_transmit_message(
    tss_instance_t* const instance,
    uint8_t channel,
    uint16_t const iden,
    uint8_t *const data,
    uint8_t const data_size,
    uint8_t const memory_offset,
    uint8_t const rak);

int tss_reply_request_message(
    tss_instance_t* const instance,
    uint8_t channel,
    uint16_t const iden,
    uint8_t const data_size,
    uint8_t const memory_offset);

int tss_immediate_reply_message(
    tss_instance_t* const instance,
    uint8_t channel,
    uint16_t const iden,
    uint8_t *const data,
    uint8_t const data_size,
    uint8_t const memory_offset);

int tss_deferred_reply_message(
    tss_instance_t* const instance,
    uint8_t channel,
    uint16_t const iden,
    uint8_t *const data,
    uint8_t const data_size,
    uint8_t const memory_offset);

/* ================================================================ */

/**
 * @brief Create and allocate resources for use with the TSS463C.
 * Initializes the SPI driver.
 * 
 * @param instance 
 * @return int 
 */
int tss_create(tss_instance_t* instance);

/**
 * @brief Reset the TSS463C and put it in active mode.
 * 
 * @param instance 
 * @return int 
 */
int tss_start(
    tss_instance_t* const instance);

void tss_task(void *params);

/* ================================================================ */

int tss_activate(tss_instance_t *instance);
int tss_sleep(tss_instance_t *instance);
int tss_idle(tss_instance_t *instance);

#endif