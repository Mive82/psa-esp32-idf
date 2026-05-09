#ifndef _MIVE_VAN_RD3_COMMAND_STRUCT_H
#define _MIVE_VAN_RD3_COMMAND_STRUCT_H

#include <stdint.h>

#include "van_structs_common.h"

// Iden 0x8d4

enum psa_van_rd3_source
{
  RD3_SOURCE_NONE = 0,
  RD3_SOURCE_TUNER = 0x01,
  RD3_SOURCE_CD_TAPE = 0x02,
  RD3_SOURCE_CDC = 0x03,
  RD3_SOURCE_NAVIGATION = 0x05,
};

// This is data[1] when data[0] == 0x11
struct psa_van_rd3_state_change_byte
{
  uint8_t mute : 1;
  uint8_t auto_volume : 1;
  uint8_t : 2;
  uint8_t loudness : 1;
  // Tells the radio to report all key presses instead of handling them
  uint8_t keyboard_override : 1;
  uint8_t power : 1;
  uint8_t key : 1;
} __attribute__((packed));

struct psa_van_rd3_command_update_state
{
  uint8_t command_type; // 0x11
  union {
    struct psa_van_rd3_state_change_byte data;
    uint8_t byte;
  } data;
} __attribute__((packed));

struct psa_van_rd3_command_change_source
{
  uint8_t command_type; // 0x12
  uint8_t source; // enum psa_van_rd3_source
} __attribute__((packed));

struct psa_van_rd3_command_change_audio_settings
{
  uint8_t command_type; // 0x14
  struct psa_radio_settings_values balance; // The 7th is set when the audio menu is closing.
  struct psa_radio_settings_values fader;
  struct psa_radio_settings_values bass;
  struct psa_radio_settings_values treble;
} __attribute__((packed));

// Probably used to specify preset that `554 d3` returns
struct psa_van_rd3_command_request_preset
{
  uint8_t command_type; // 0x27
  uint8_t memory_position : 4;
  uint8_t band : 4;
} __attribute__((packed));

// Probably used to control CD playback
struct psa_van_rd3_command_cd
{
  uint8_t command_type; // 0x61
  uint8_t play_pause; // 0x02 - pause, 0x03 - play, TODO
  uint8_t byte2; // Unknown
  uint8_t byte3; // 0xff - Next, 0xfe - Prev, TODO
} __attribute__((packed));

#endif // _MIVE_VAN_RD3_COMMAND_STRUCT_H