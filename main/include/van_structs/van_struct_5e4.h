#ifndef _MIVE_VAN_STRUCT_5E4_H
#define _MIVE_VAN_STRUCT_5E4_H

#include <stdint.h>

// IDEN 0x5e4 or PSA_VAN_IDEN_MFD_STATUS

struct psa_van_5e4_struct
{
  uint8_t unused : 2;                         // bits 0 and 1
  uint8_t alert_reminder_request : 1;         // bit 2
  uint8_t overspeed_memorization_request : 1; // bit 3
  uint8_t active_overspeed_alerts : 1;        // bit 4
  uint8_t power_keep_alive : 1;               // bit 5
  uint8_t reset_trip_b_request : 1;           // bit 6
  uint8_t reset_trip_a_request : 1;           // bit 7

  uint8_t overspeed_alert_value;
} __attribute__((packed));

#endif // _MIVE_VAN_STRUCT_5E4_H