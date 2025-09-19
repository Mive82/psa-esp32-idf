#include "freertos/FreeRTOS.h"

#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "include/van/tss463_regs.h"
#include "include/van/tss463c.h"
#include "esp_log.h"

#include "include/mive/common.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "include/global_config.h"

static const char *TAG = "tss463c";

static portMUX_TYPE tss_spinlock = portMUX_INITIALIZER_UNLOCKED;

#define get_bytes_from_iden(iden, byte1, byte2) \
    byte1 = (uint8_t)(((iden << 4) & 0xff00) >> 8); \
    byte2 = (uint8_t)(iden & 0xF);

IRAM_ATTR static void pre_transaction_cb(spi_transaction_t *trans)
{
    return;
}

IRAM_ATTR static void post_transaction_cb(spi_transaction_t *trans)
{
    return;
}

// IRAM_ATTR static void get_bytes_from_iden(uint16_t iden, uint8_t* byte1, uint8_t* byte2)
// {
//     *byte1 = (uint8_t)(((iden << 4) & 0xff00) >> 8);
//     *byte2 = (uint8_t)(iden & 0xF);
// }

IRAM_ATTR static int is_channel_valid(tss_instance_t* instance, uint8_t channel, uint16_t iden)
{
    tss_channel_t *chan = NULL;

    if(channel > TSS_MAX_CHANNEL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }
    
    // chan = &instance->channels[channel];

    // if((!chan->is_busy) && ((chan->is_occupied == 0) || (chan->is_occupied && chan->identifier == iden)))
    // {
    //     return MIVE_OK;
    // }

    // return -MIVE_ERR_CHANNEL_BUSY;
    return MIVE_OK;
}

static int tss_spi_master_init(tss_instance_t *instance)
{
    spi_device_handle_t tss_handle;

    #if (ESP32_BOARD_TYPE == DEVKIT)

    spi_bus_config_t spi_config = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .max_transfer_sz = 0,
        .data2_io_num = -1,
        .data3_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1
    };

    #else

    spi_bus_config_t spi_config = {
        .mosi_io_num = 13,
        .miso_io_num = 12,
        .sclk_io_num = 14,
        .max_transfer_sz = 0,
        .data2_io_num = -1,
        .data3_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1
    };

    #endif

    spi_device_interface_config_t device_config = {
        .clock_speed_hz = 4000000,
        .mode = 3,
        .spics_io_num = -1,
        .queue_size = 1,
        .pre_cb = pre_transaction_cb,
        .post_cb = post_transaction_cb,

    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &spi_config, SPI_DMA_CH_AUTO));

    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &device_config, &tss_handle));

    instance->tss_handle = tss_handle;

    return 0;
}

/**
 * @brief Performs an Asynchronous Reset and puts the TSS into motorolla mode
 * 
 * @param instance 
 * @return int 
 */
static int tss_motorolla_mode(tss_instance_t *instance)
{
    int return_val = 0;
    // gpio_set_level(instance->cs_pin, 0);
    spi_device_acquire_bus(instance->tss_handle, portMAX_DELAY);
    TSS_SELECT();

    usleep(1);
    spi_transaction_t transaction;
    memset(&transaction, 0, sizeof(transaction));
    transaction.user = instance;
    transaction.tx_data[0] = 0x00;
    transaction.length = 8;
    transaction.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;

    ESP_ERROR_CHECK(spi_device_transmit(instance->tss_handle, &transaction));

    if (transaction.rx_data[0] != 0xAA)
    {
        printf("Error setting motorolla mode: Invalid response 0x%02X / 0xAA\n", transaction.rx_data[0]);
        return_val = 1;
        // goto error;
    }

    usleep(4);

    transaction.user = instance;
    transaction.tx_data[0] = 0x00;
    transaction.length = 8;
    transaction.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;

    ESP_ERROR_CHECK(spi_device_transmit(instance->tss_handle, &transaction));

    if (transaction.rx_data[0] != 0x55)
    {
        printf("Error setting motorolla mode: Invalid response 0x%02X / 0x55\n", transaction.rx_data[0]);
        return_val = 1;
        // goto error;
    }

    usleep(2);

    // error:
    // gpio_set_level(instance->cs_pin, 1);
    TSS_DESELECT();
    spi_device_release_bus(instance->tss_handle);

    instance->chip_mode = TSS_MODE_IDLE;

    return return_val;
}

static int tss_disable_channel(tss_instance_t *instance, uint8_t channel_id)
{
    ESP_LOGI(TAG, "[%s] Disabling channel %d", __func__, channel_id);

    int return_val = 0;
    if(channel_id > TSS_MAX_CHANNEL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }

    /*
     *  Datasheet pg. 37
     *  The easiest way to disable an channel register is to set the
     *  received and transmitted bits to 1 in the Message Length and Status Registe
     */
    static const uint8_t data[] = {0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00};

    return_val = tss_registers_set(instance, TSS_CHANNEL_ADDR(channel_id), data, sizeof(data));

    memset(&instance->channels[channel_id], 0, sizeof(instance->channels[channel_id]));

    return return_val;
}

/**
 * @brief Put the TSS in active mode
 *
 * @param instance
 * @return int
 */
int tss_activate(tss_instance_t *instance)
{
    // Sending the Activate command
    command_register_t comm_register = {0};
    comm_register.data.ACTI = 1;

    if (tss_register_set(instance, TSS_COMMANDREGISTER, comm_register.Value) != MIVE_OK)
    {
        return 1;
    }

    instance->chip_mode = TSS_MODE_ACTIVE;

    return 0;
}

/**
 * @brief Put the TSS in sleep mode
 *
 * @param instance
 * @return int
 */
int tss_sleep(tss_instance_t *instance)
{
    // Sending the Sleep command
    command_register_t comm_register = {0};
    comm_register.data.SLEEP = 1;

    if (tss_register_set(instance, TSS_COMMANDREGISTER, comm_register.Value))
    {
        return 1;
    }

    instance->chip_mode = TSS_MODE_SLEEP;

    return 0;
}

/**
 * @brief Put the TSS in idle mode
 *
 * @param instance
 * @return int
 */
int tss_idle(tss_instance_t *instance)
{
    // Sending the Idle command
    command_register_t comm_register = {0};
    comm_register.data.IDLE = 1;

    if (tss_register_set(instance, TSS_COMMANDREGISTER, comm_register.Value))
    {
        return 1;
    }

    instance->chip_mode = TSS_MODE_IDLE;

    return 0;
}

IRAM_ATTR int tss_register_set(
    tss_instance_t *instance,
    uint8_t reg_addr,
    uint8_t reg_value)
{
    int return_val = 0;
    spi_transaction_t transaction = {0};

    if(unlikely(instance->chip_mode == TSS_MODE_SLEEP))
    {
        ESP_LOGE(TAG, "[%s] Chip in sleep mode", __func__);
        return -1;
    }

    xSemaphoreTake(instance->tss_spi_semaphore, portMAX_DELAY);
    spi_device_acquire_bus(instance->tss_handle, portMAX_DELAY);

    // gpio_set_level(instance->cs_pin, 0);
    TSS_SELECT();
    transaction.user = instance;
    transaction.tx_data[0] = reg_addr;
    transaction.length = 8;
    transaction.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;

    usleep(1);

    ESP_ERROR_CHECK(spi_device_transmit(instance->tss_handle, &transaction));

    if (transaction.rx_data[0] != 0xAA)
    {
        printf("Error setting regiser: Invalid response 0x%02x / 0xAA\n", transaction.rx_data[0]);
        return_val = 1;
        goto error;
    }

    transaction.tx_data[0] = TSS_WRITE;
    transaction.length = 8;
    transaction.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;

    usleep(2);
    ESP_ERROR_CHECK(spi_device_transmit(instance->tss_handle, &transaction));

    if (transaction.rx_data[0] != 0x55)
    {
        printf("Error setting regiser: Invalid response 0x%02x / 0x55\n", transaction.rx_data[0]);
        return_val = 1;
        goto error;
    }

    usleep(4);

    transaction.tx_data[0] = reg_value;
    transaction.length = 8;
    transaction.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;

    ESP_ERROR_CHECK(spi_device_transmit(instance->tss_handle, &transaction));

error:
    TSS_DESELECT();
    spi_device_release_bus(instance->tss_handle);
    xSemaphoreGive(instance->tss_spi_semaphore);

    return return_val;
}

IRAM_ATTR int tss_registers_set(
    tss_instance_t *const instance,
    uint8_t const reg_addr,
    uint8_t const *const values,
    uint8_t const count)
{
    unsigned int i = 0;
    int return_val = 0;
    spi_transaction_t transaction = {0};

    xSemaphoreTake(instance->tss_spi_semaphore, portMAX_DELAY);
    // printf("Setting %d registers starting from addr %d\n", count, reg_addr);
    spi_device_acquire_bus(instance->tss_handle, portMAX_DELAY);
    TSS_SELECT();

    transaction.user = instance;
    transaction.tx_data[0] = reg_addr;
    transaction.length = 8;
    transaction.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;

    usleep(1);
    spi_device_transmit(instance->tss_handle, &transaction);
    if (transaction.rx_data[0] != 0xAA)
    {
        printf("Error setting regiser: Invalid response 0x%02x / 0xAA\n", transaction.rx_data[0]);
        return_val = 1;
        goto error;
    }

    transaction.tx_data[0] = TSS_WRITE;

    usleep(2);
    spi_device_transmit(instance->tss_handle, &transaction);

    if (transaction.rx_data[0] != 0x55)
    {
        printf("Error setting regiser: Invalid response 0x%02x / 0x55\n", transaction.rx_data[0]);
        return_val = 1;
        goto error;
    }

    usleep(4);

    // printf("Transmitting: ");
    for (i = 0; i < count; ++i)
    {
        transaction.tx_data[0] = values[i];
        spi_device_transmit(instance->tss_handle, &transaction);
        printf("%02x ", transaction.tx_data[0]);
        usleep(3);
    }

    printf("\n");

    usleep(3);

error:

    TSS_DESELECT();
    spi_device_release_bus(instance->tss_handle);

    xSemaphoreGive(instance->tss_spi_semaphore);

    return return_val;
}

IRAM_ATTR int tss_register_get(
    tss_instance_t *const instance,
    uint8_t const reg_addr,
    uint8_t *const reg_value)
{
    int return_val = 0;
    spi_transaction_t transaction = {0};
    
    xSemaphoreTake(instance->tss_spi_semaphore, portMAX_DELAY);
    spi_device_acquire_bus(instance->tss_handle, portMAX_DELAY);

    // gpio_set_level(instance->cs_pin, 0);
    TSS_SELECT();
    transaction.user = instance;
    transaction.tx_data[0] = reg_addr;
    transaction.length = 8;
    transaction.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;

    usleep(1);

    ESP_ERROR_CHECK(spi_device_transmit(instance->tss_handle, &transaction));

    if (transaction.rx_data[0] != 0xAA)
    {
        printf("Error getting regiser: Invalid response 0x%02x / 0xAA\n", transaction.rx_data[0]);
        return_val = 1;
        goto error;
    }

    transaction.tx_data[0] = TSS_READ;

    usleep(2);
    ESP_ERROR_CHECK(spi_device_transmit(instance->tss_handle, &transaction));

    if (transaction.rx_data[0] != 0x55)
    {
        printf("Error getting regiser: Invalid response 0x%02x / 0x55\n", transaction.rx_data[0]);
        return_val = 1;
        goto error;
    }

    usleep(4);

    transaction.tx_data[0] = 0xFF;
    ESP_ERROR_CHECK(spi_device_transmit(instance->tss_handle, &transaction));

    *reg_value = transaction.rx_data[0];
error:
    TSS_DESELECT();
    spi_device_release_bus(instance->tss_handle);

    xSemaphoreGive(instance->tss_spi_semaphore);

    return return_val;
}

IRAM_ATTR int tss_registers_get(
    tss_instance_t *const instance,
    uint8_t const reg_addr,
    uint8_t *const buffer,
    uint8_t count)
{
    int return_val = 0;
    spi_transaction_t transaction = {0};
    
    if (unlikely(buffer == NULL))
    {
        return 1;
    }
    
    if(unlikely(instance->chip_mode == TSS_MODE_SLEEP))
    {
        ESP_LOGE(TAG, "[%s] Chip in sleep mode", __func__);
        return -1;
    }

    xSemaphoreTake(instance->tss_spi_semaphore, portMAX_DELAY);
    spi_device_acquire_bus(instance->tss_handle, portMAX_DELAY);

    // gpio_set_level(instance->cs_pin, 0);
    TSS_SELECT();
    memset(&transaction, 0, sizeof(transaction));
    transaction.user = instance;
    transaction.tx_data[0] = reg_addr;
    transaction.length = 8;
    transaction.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;

    usleep(1);

    ESP_ERROR_CHECK(spi_device_transmit(instance->tss_handle, &transaction));

    if (transaction.rx_data[0] != 0xAA)
    {
        printf("Error setting regiser: Invalid response 0x%02x / 0xAA\n", transaction.rx_data[0]);
        return_val = 1;
        goto error;
    }

    transaction.tx_data[0] = TSS_READ;

    usleep(2);
    ESP_ERROR_CHECK(spi_device_transmit(instance->tss_handle, &transaction));

    if (transaction.rx_data[0] != 0x55)
    {
        printf("Error setting regiser: Invalid response 0x%02x / 0x55\n", transaction.rx_data[0]);
        return_val = 1;
        goto error;
    }

    usleep(4);

    for (int i = 0; i < count; ++i)
    {

        transaction.tx_data[0] = 0xFF;
        ESP_ERROR_CHECK(spi_device_transmit(instance->tss_handle, &transaction));
        buffer[i] = transaction.rx_data[0];
        usleep(3);
    }

error:
    TSS_DESELECT();
    spi_device_release_bus(instance->tss_handle);

    xSemaphoreGive(instance->tss_spi_semaphore);

    return return_val;
}

uint8_t tss_get_channel_status_register(
    tss_instance_t* const instance,
    uint8_t const channel_num)
{
    uint8_t reg_value;
    if(channel_num > TSS_MAX_CHANNEL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }
    tss_register_get(instance, TSS_CHANNEL_MESS_LEN_AND_STATUS(channel_num), &reg_value);

    instance->channels[channel_num].message_len_and_status_reg_value = reg_value;

    return reg_value;
}

uint8_t tss_set_channel_status_register(
    tss_instance_t* const instance,
    uint8_t const channel_num,
    uint8_t const reg_value)
{
    uint8_t new_reg_value;
    if(channel_num > TSS_MAX_CHANNEL)
    {
        return -MIVE_ERR_INVALID_ARGUMENT;
    }
    tss_register_set(instance, TSS_CHANNEL_MESS_LEN_AND_STATUS(channel_num), reg_value);

    tss_register_get(instance, TSS_CHANNEL_MESS_LEN_AND_STATUS(channel_num), &new_reg_value);

    instance->channels[channel_num].message_len_and_status_reg_value = new_reg_value;

    return new_reg_value;
}

int tss_abort_channel(
    tss_instance_t* const instance,
    uint8_t const channel_num)
{
    message_length_and_status_register_t reg;

    reg.Value = tss_get_channel_status_register(instance, channel_num);
    reg.data.CHER = 1;

    tss_set_channel_status_register(instance, channel_num, reg.Value);

    return MIVE_OK;
}

int tss_free_channel(
    tss_instance_t* const instance,
    uint8_t const channel_num)
{
    tss_channel_t* channel = &instance->channels[channel_num];

    channel->is_busy = false;
    tss_disable_channel(instance, channel_num);
    return MIVE_OK;
}

int tss_create(tss_instance_t* instance)
{
    if (unlikely(instance == NULL))
    {
        printf("tss_start -> instance is NULL\n");
        return 1;
    }

    memset(instance, 0, sizeof(*instance));

    if (tss_spi_master_init(instance))
    {
        return 1;
    }

    instance->cs_pin = TSS_CS_PIN;
    instance->buffers_size = 256;
    instance->interrupt_pin = TSS_INT_PIN;
    instance->tss_spi_semaphore = xSemaphoreCreateMutex();
    instance->chip_mode = TSS_MODE_IDLE;

    xSemaphoreGive(instance->tss_spi_semaphore);

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT_OD,
        .pin_bit_mask = (1 << instance->cs_pin),
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };

    // Configure CS pin as output
    gpio_config(&io_conf);

    TSS_DESELECT();

    instance->tx_buffer = heap_caps_calloc(1, instance->buffers_size, MALLOC_CAP_DMA);
    instance->rx_buffer = heap_caps_calloc(1, instance->buffers_size, MALLOC_CAP_DMA);

    return 0;
}

int tss_start(tss_instance_t *instance)
{

    if (tss_motorolla_mode(instance))
    {
        // return 1;
    }
    usleep(3);

    for (uint8_t i = 0; i < TSS_MAX_CHANNEL; i++)
    {
        tss_disable_channel(instance, i);
    }

    if (tss_register_set(instance, TSS_LINECONTROL, TSS_8MHz_125kBPS))
    {
        return 1;
    }

    transmit_control_register_t tc_register;
    tc_register.Value = 0;
    tc_register.data.max_retries = 5;
    tc_register.data.VER = 1;
    tc_register.data.module_type = 1;

    if (tss_register_set(instance, TSS_TRANSMITCONTROL, tc_register.Value))
    {
        return 1;
    }

    // Select interrupts to enable
    interrupt_register_t it_register;
    it_register.Value = 0;
    it_register.data.RESET = 1; // Must be 1
    it_register.data.TOK = 0;   // Change this to enable interrupt on successful transmissions
    it_register.data.ROK = 0;   // Change this to enable interrupt on reception with RAK
    it_register.data.RNOK = 0;  // Change this to enable interrupt on reception without RAK
    it_register.data.RE = 0;    // Change this to enable interrupt on reception error
    it_register.data.TE = 0;    // Change this to enable interrupt on transmission error

    if (tss_register_set(instance, TSS_INTERRUPTENABLE, it_register.Value))
    {
        return 1;
    }

    tss_register_get(instance, TSS_INTERRUPTSTATUS, &(it_register.Value));

    printf("\tRNOK: %d\n", it_register.data.RNOK);
    printf("\tROK: %d\n", it_register.data.ROK);
    printf("\tRE: %d\n", it_register.data.RE);
    printf("\tTOK: %d\n", it_register.data.TOK);
    printf("\tTE: %d\n", it_register.data.TE);
    printf("\tRESET: %d\n", it_register.data.RESET);

    // Reset all interrupt statuses
    it_register.data.RESET = 1;
    it_register.data.TOK = 1;
    it_register.data.ROK = 1;
    it_register.data.RNOK = 1;
    it_register.data.RE = 1;
    it_register.data.TE = 1;

    if (tss_register_set(instance, TSS_INTERRUPTRESET, it_register.Value))
    {
        return 1;
    }

    // Memsetting the TSS's RAM to zeros
    uint8_t zeroarray[128] = {0};
    if (tss_registers_set(instance, TSS_GETMAIL(0), zeroarray, 128))
    {
        return 1;
    }

    it_register.Value = 0;
    it_register.data.RESET = 1; // Must be 1
    it_register.data.TOK = 1;   // Change this to enable interrupt on successful transmissions
    it_register.data.ROK = 1;   // Change this to enable interrupt on reception with RAK
    it_register.data.RNOK = 1;  // Change this to enable interrupt on reception without RAK
    it_register.data.RE = 1;    // Change this to enable interrupt on reception error
    it_register.data.TE = 1;    // Change this to enable interrupt on transmission error

    if (tss_register_set(instance, TSS_INTERRUPTENABLE, it_register.Value))
    {
        return 1;
    }

    if (tss_activate(instance))
    {
        return 1;
    }

    return 0;
}

int tss_transmit_message(
    tss_instance_t* const instance,
    uint8_t channel,
    uint16_t const iden,
    uint8_t *const data,
    uint8_t const data_size,
    uint8_t const memory_offset,
    uint8_t const rak)
{
    uint8_t id1 = 0;
    uint8_t id2 = 0;
    uint8_t memory_address;
    id2_and_command_register_t id2_reg = {0};
    message_pointer_register_t mp_reg = {0};
    message_length_and_status_register_t mls_reg = {0};
    uint8_t channel_data[8] = {0};
    tss_channel_t* chan = NULL;
    int retval;
    
    retval = is_channel_valid(instance, channel, iden);

    if(retval != MIVE_OK)
    {
        printf("[%s] Invalid channel %d (%d)\n", __func__, channel, retval);
        return retval;
    }
    if(memory_offset & 0x80)
    {
        memory_address = memory_offset;
    } 
    else
    {
        memory_address = TSS_GETMAIL(memory_offset);
    }

    ESP_LOGD(TAG, "[%s] Using memory addr 0x%02x", __func__, memory_address);

    get_bytes_from_iden(iden, id1, id2);


    id2_reg.data.ID = id2;
    id2_reg.data.RNW = 0;
    id2_reg.data.RTR = 0;
    id2_reg.data.EXT = 1;
    id2_reg.data.RAK = rak;

    mp_reg.data.DRAK = 0;
    mp_reg.data.message_pointer = memory_address & 0x7F;

    mls_reg.data.CHTx = 0;
    mls_reg.data.M_L = data_size + 1;

    // When writing to the bus, the first element in the buffer is ignored,
    // so we skip one (memory_address + 1)
    tss_registers_set(instance, memory_address + 1, data, data_size);
    
    channel_data[0] = id1;
    channel_data[1] = id2_reg.Value;
    channel_data[2] = mp_reg.Value;
    channel_data[3] = mls_reg.Value;
    channel_data[6] = id1;
    channel_data[7] = id2;
    
    tss_registers_set(instance, TSS_CHANNEL_ADDR(channel), channel_data, 8);

    chan = &instance->channels[channel];
    chan->data_size = data_size + 1;
    chan->ram_address = memory_address;
    chan->identifier = iden;
    chan->is_occupied = true;
    chan->is_busy = true;
    chan->message_len_and_status_reg_value = mls_reg.Value;
    chan->tx_attempt = 1;
    chan->message_type = TSS_TRANSMIT;

    return MIVE_OK;
}

int tss_immediate_reply_message(
    tss_instance_t* const instance,
    uint8_t channel,
    uint16_t const iden,
    uint8_t *const data,
    uint8_t const data_size,
    uint8_t const memory_offset)
{
    uint8_t id1 = 0;
    uint8_t id2 = 0;
    uint8_t memory_address;
    id2_and_command_register_t id2_reg = {0};
    message_pointer_register_t mp_reg = {0};
    message_length_and_status_register_t mls_reg = {0};
    uint8_t channel_data[8] = {0};
    tss_channel_t* chan = NULL;
    int retval;
    
    retval = is_channel_valid(instance, channel, iden);

    if(retval != MIVE_OK)
    {
        printf("[%s] Invalid channel %d (%d)\n", __func__, channel, retval);
        return retval;
    }

    if(memory_offset & 0x80)
    {
        memory_address = memory_offset;
    } 
    else
    {
        memory_address = TSS_GETMAIL(memory_offset);
    }

    ESP_LOGD(TAG, "[%s] Using memory addr 0x%02x", __func__, memory_address);

    get_bytes_from_iden(iden, id1, id2);


    id2_reg.data.ID = id2;
    id2_reg.data.RNW = 1;
    id2_reg.data.RTR = 0;
    id2_reg.data.EXT = 1;
    id2_reg.data.RAK = 0;

    mp_reg.data.DRAK = 0;
    mp_reg.data.message_pointer = memory_address & 0x7F;

    mls_reg.data.CHTx = 0;
    mls_reg.data.CHRx = 0;
    mls_reg.data.M_L = data_size + 1;

    // When writing to the bus, the first element in the buffer is ignored,
    // so we skip one (memory_address + 1)
    tss_registers_set(instance, memory_address + 1, data, data_size);
    
    channel_data[0] = id1;
    channel_data[1] = id2_reg.Value;
    channel_data[2] = mp_reg.Value;
    channel_data[3] = mls_reg.Value;
    channel_data[6] = id1;
    channel_data[7] = id2;
    
    tss_registers_set(instance, TSS_CHANNEL_ADDR(channel), channel_data, 8);

    chan = &instance->channels[channel];
    chan->data_size = data_size + 1;
    chan->ram_address = memory_address;
    chan->identifier = iden;
    chan->is_occupied = true;
    chan->is_busy = true;
    chan->message_len_and_status_reg_value = mls_reg.Value;
    chan->tx_attempt = 1;
    chan->message_type = TSS_IMM_REPLY;

    return MIVE_OK;
}