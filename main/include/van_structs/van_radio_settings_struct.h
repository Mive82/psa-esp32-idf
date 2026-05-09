#ifndef PSA_VAN_RADIO_INFO_STRUCT_H
#define PSA_VAN_RADIO_INFO_STRUCT_H

#include <stdint.h>

#include "van_structs_common.h"
// Radio audio settings 0x4D4 PSA_VAN_IDEN_AUDIO_SETTINGS

#define PSA_RADIO_INFO_SOURCE_RADIO 0x51;
#define PSA_RADIO_INFO_SOURCE_CD 0x52;

struct psa_radio_settings_byte_1
{
    uint8_t stalk_mute : 1;
    uint8_t external_mute : 1;
    uint8_t auto_volume : 1;
    uint8_t : 1;
    uint8_t loudness_on : 1;
    // Tells the radio to report all key presses instead of handling them
    uint8_t keyboard_override : 1;
    uint8_t : 2;
} __attribute__((packed));

struct psa_radio_settings_byte_2
{
    uint8_t power_on : 1;
    uint8_t request_power_on : 1; // Appears after 0x8c4 Radio event
    uint8_t request_power_off : 1; // Appears after 0x8c4 Radio event
    uint8_t : 4;
    uint8_t ready : 1; // Disappear and reapperas after sending "Power on" command
} __attribute__((packed));

struct psa_radio_settings_byte_4
{
    uint8_t source : 4;
    uint8_t : 1;
    uint8_t tape_present : 1;
    uint8_t cd_present : 1;
    uint8_t : 1;
} __attribute__((packed));

/**
 * @brief Radio info struct. Ident 0x4D4
 *
 */
struct psa_van_radio_settings
{
    uint8_t header;
    struct psa_radio_settings_byte_1 audio_properties;
    struct psa_radio_settings_byte_2 power;
    uint8_t field3;
    struct psa_radio_settings_byte_4 source;
    struct psa_radio_settings_values volume;
    struct psa_radio_settings_values balance;
    struct psa_radio_settings_values fader;
    struct psa_radio_settings_values bass;
    struct psa_radio_settings_values treble;
    uint8_t footer;
} __attribute__((packed));

#endif // PSA_VAN_RADIO_INFO_STRUCT_H