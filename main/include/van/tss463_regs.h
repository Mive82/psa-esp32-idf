#ifndef TSS463C_REGS_H
#define TSS463C_REGS_H

#include <stdint.h>

// Register addresses

#define TSS_LINECONTROL 0x00       // r/w   - 0x00
#define TSS_TRANSMITCONTROL 0x01   // r/w   - 0x02
#define TSS_DIAGNOSISCONTROL 0x02  // r/w   - 0x00
#define TSS_COMMANDREGISTER 0x03   // w     - 0x00
#define TSS_LINESTATUS 0x04        // r     - 0bx01xxx00
#define TSS_TRANSMITSTATUS 0x05    // r     - 0x00
#define TSS_LASTMESSAGESTATUS 0x06 // r     - 0x00
#define TSS_LASTERRORSTATUS 0x07   // r     - 0x00

/* Interrupt registers */

#define TSS_INTERRUPTSTATUS 0x09   // r     - 0x80
#define TSS_INTERRUPTENABLE 0x0A   // r/w   - 0x80
#define TSS_INTERRUPTRESET 0x0B    // w

// Used with Interrupt Status regiter, Interrupt enable register and Interrupt reset register
typedef union
{
    struct
    {
        // Receive "with no RAK (RAK=0)" OK Status Flag
        uint8_t RNOK : 1;
        // Receive "with RAK (RAK=1)" OK Status Flag
        uint8_t ROK : 1;
        // Receive Error Status Flag
        uint8_t RE : 1;
        // Transmit OK Status Flag
        uint8_t TOK : 1;
        // Transmit Error Status Flag (or Exceeded Retry)
        uint8_t TE : 1;
        // Must be 0
        uint8_t ZERO : 2;
        // Interrupt on chip reset, must be 1 in interrupt enable
        uint8_t RESET : 1;
    } data;
    uint8_t Value;
} interrupt_register_t;

typedef union
{
    struct
    {
        uint8_t CHRx : 1; // Channel message received
        uint8_t CHTx : 1; // Channel message transmitted
        /*
        As status, this bit is set by the TSS463C when error occurs in transmission or on a received frame. The user must reset it
        */
        uint8_t CHER : 1;
        /*
        The 5 high bits of this register allows the user to specify either the length of the message to be transmitted, or the maximum length of a message receivable in the pointed reception buffer
        The first byte in this register does not contain data, but the length of the message received. This implies that the length value has to be equal to or greater than the maximum length of a message
        to be received in this buffer (or the length of a message to be transmitted) plus 1
        */
        uint8_t M_L : 5;
    } data;
    uint8_t Value;
} message_length_and_status_register_t;

typedef union
{
    struct
    {
        // Length of the received message
        uint8_t received_message_len : 5;
        // Status of the received RTR bit
        uint8_t RTR : 1;
        // Status of the received RNW bit
        uint8_t RNW : 1;
        // Status of the received RAK bit
        uint8_t RRAK : 1;
    } data;
    uint8_t Value;
} received_message_and_status_register_t;

typedef union
{
    struct
    {
        uint8_t RTR : 1;
        uint8_t RNW : 1;
        uint8_t RAK : 1;
        uint8_t EXT : 1;
        uint8_t ID : 4;
    } data;
    uint8_t Value;
} id2_and_command_register_t;

typedef union
{
    struct
    {
        // Message Pointer the value in this register is the offset from 0x80
        uint8_t message_pointer : 7;
        /*Disable RAK (Used in 'Spy Mode')
        In reception: whatever is the RAK bit of the incoming valid frame, no ACK answer will be set. If the message was successfully received, an IT is set (ROK or RNOK).
        In transmission: no action.
        One: disable active, 'spy mode'.
        Zero: disable inactive, normal operation
        */
        uint8_t DRAK : 1;
    } data;
    uint8_t Value;
} message_pointer_register_t;

typedef union
{
    struct
    {
        // Module type:
        // 1 - Master/autonomous module (rank 0,1, and 16)
        // 0 - Synchronous/slave module (rank 1 and 16)
        uint8_t module_type : 1;
        // Must be 0b001
        uint8_t VER : 3;
        // Maximum retries
        uint8_t max_retries : 4;
    } data;
    uint8_t Value;
} transmit_control_register_t;

typedef union
{
    struct
    {
        uint8_t ESDC : 1;
        uint8_t ETIP : 1;
        uint8_t Mb : 1;
        uint8_t Ma : 1;
        uint8_t SDC : 4;
    } data;
    uint8_t Value;
} diagnosis_control_register_t;

// Page 30
typedef union
{
    struct
    {
        uint8_t MSDC : 1;
        uint8_t ZERO : 2; // Must be 0
        // Rearbitrate -> Reset the retry counter and rearbitrate all messages
        uint8_t REAR : 1;
        // Activate command -> Page 51
        uint8_t ACTI : 1;
        // Idle command -> Page 51
        uint8_t IDLE : 1;
        // Sleep command -> puts the chip to sleep
        uint8_t SLEEP : 1;
        // General reset -> resets the chip
        uint8_t GRES : 1;
    } data;
    uint8_t Value;
} command_register_t;

// Page 31
typedef union
{
    struct
    {
        // Receiving status -> 1 when there is activity on the bus
        uint8_t RXG : 1;
        // Transmitting status -> TSS is transimitting a message
        uint8_t TXG : 1;
        // System diagnostics bits -> Table 8
        uint8_t Sa : 1;
        uint8_t Sb : 1;
        uint8_t Sc : 1;
        // Chip is in Idle state
        uint8_t IDG : 1;
        // Chip is in sleep state
        uint8_t SPG : 1;
        uint8_t : 1;
    } data;
    uint8_t Value;
} line_status_register_t;

// Page 32
typedef union
{
    struct
    {
        // Identifier currently transmitting
        uint8_t IDT : 4;
        // Number of retries done
        uint8_t NRT : 4;
    } data;
    uint8_t Value;
} transmission_status_register_t;

// Page 32
typedef union
{
    struct
    {
        // Identifier transmitted, received, or exceeded the retry count
        uint8_t IDTr : 4;
        // Number of retries done
        uint8_t NRTr : 4;
    } data;
    uint8_t Value;
} last_message_status_register_t;

// Page 32
typedef union
{
    struct
    {
        uint8_t frame_violation : 1;
        uint8_t code_violation : 1;
        uint8_t ack_error : 1;
        uint8_t crc_error : 1;
        uint8_t : 1;
        uint8_t buffer_overflow : 1;
        uint8_t buffer_occupied : 1;
        uint8_t : 1;
    } data;
    uint8_t Value;
} last_error_status_register_t;

#endif