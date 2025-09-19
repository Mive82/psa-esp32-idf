#ifndef VAN_PACKET_PARSER_H
#define VAN_PACKET_PARSER_H

#include <stdint.h>

#include "../mive/psa_packet_defs.h"

struct psa_output_data_buffers
{
    struct psa_engine_data *engine_data; // PSA_IDENT_ENGINE
    struct psa_radio_data *radio_data; // PSA_IDENT_RADIO
    struct psa_preset_data *presets_data_am; // PSA_IDENT_RADIO_PRESETS
    struct psa_preset_data *presets_data_fm_1; // PSA_IDENT_RADIO_PRESETS
    struct psa_preset_data *presets_data_fm_2; // PSA_IDENT_RADIO_PRESETS
    struct psa_preset_data *presets_data_fm_ast; // PSA_IDENT_RADIO_PRESETS
    struct psa_cd_player_data *cd_player_data; // PSA_IDENT_CD_PLAYER
    struct psa_headunit_data *headunit_data; // PSA_IDENT_HEADUNIT
    struct psa_door_data *door_data; // PSA_IDENT_DOORS
    uint8_t *vin_data; // PSA_IDENT_VIN
    struct psa_dash_data *dash_data; // PSA_IDENT_DASHBOARD
    struct psa_trip_data *trip_data; // PSA_IDENT_TRIP
    struct psa_status_data *status_data; // PSA_IDENT_CAR_STATUS
};

extern int psa_parse_van_packet(
    uint16_t iden,
    uint8_t size,
    uint8_t const *const data,
    struct psa_output_data_buffers *data_buffers);

#endif // VAN_PACKET_PARSER_H