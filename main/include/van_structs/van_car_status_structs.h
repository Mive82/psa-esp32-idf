#ifndef PSA_VAN_CAR_STATUS_STRUCT_H
#define PSA_VAN_CAR_STATUS_STRUCT_H

#include <stdint.h>

// Iden 0x524

typedef struct {
    uint8_t tyre_pressure_low                      : 1; // bit 0
    uint8_t no_door_open                           : 1; // bit 1
    uint8_t automatic_gearbox_temperature_too_high : 1; // bit 2
    uint8_t brake_system_fault                     : 1; // bit 3
    uint8_t hydraulic_suspension_pressure_faulty   : 1; // bit 4
    uint8_t suspension_faulty                      : 1; // bit 5
    uint8_t engine_oil_temperature_too_high        : 1; // bit 6
    uint8_t engine_overheating                     : 1; // bit 7
} VanDisplayMsgV2Byte0Struct;

typedef struct {
    uint8_t unblock_diesel_filter     : 1; // bit 0
    uint8_t stop_car_icon             : 1; // bit 1
    uint8_t diesel_additive_too_low   : 1; // bit 2
    uint8_t fuel_tank_access_open     : 1; // bit 3
    uint8_t tyre_punctured            : 1; // bit 4
    uint8_t coolant_level_low         : 1; // bit 5
    uint8_t oil_pressure_too_low      : 1; // bit 6
    uint8_t oil_level_too_low         : 1; // bit 7
} VanDisplayMsgV2Byte1Struct;

typedef struct {
    uint8_t mil                          : 1; // bit 0  displays Antipollution fault
    uint8_t brake_pads_worn              : 1; // bit 1
    uint8_t diagnosis_ok                 : 1; // bit 2
    uint8_t automatic_gearbox_faulty     : 1; // bit 3
    uint8_t esp                          : 1; // bit 4  displays ESP faulty
    uint8_t abs                          : 1; // bit 5  displays ABS fault
    uint8_t suspension_or_steering_fault : 1; // bit 6
    uint8_t braking_system_faulty        : 1; // bit 7
} VanDisplayMsgV2Byte2Struct;

typedef struct {
    uint8_t side_airbag_faulty              : 1; // bit 0
    uint8_t airbags_faulty                  : 1; // bit 1
    uint8_t cruise_control_faulty           : 1; // bit 2
    uint8_t engine_temperature_too_high     : 1; // bit 3
    uint8_t fault_load_shedding_in_progress : 1; // bit 4
    uint8_t ambient_brightness_sensor_fault : 1; // bit 5
    uint8_t rain_sensor_fault               : 1; // bit 6
    uint8_t water_in_diesel_fuel_filter     : 1; // bit 7
} VanDisplayMsgV2Byte3Struct;

typedef struct {
    uint8_t left_rear_sliding_door_faulty  : 1; // bit 0
    uint8_t headlight_corrector_fault      : 1; // bit 1
    uint8_t right_rear_sliding_door_faulty : 1; // bit 2
    uint8_t no_broken_lamp                 : 1; // bit 3
    uint8_t battery_low                    : 1; // bit 4
    uint8_t battery_charge_fault           : 1; // bit 5
    uint8_t diesel_particle_filter_faulty  : 1; // bit 6
    uint8_t catalytic_converter_fault      : 1; // bit 7
} VanDisplayMsgV2Byte4Struct;

typedef struct {
    uint8_t handbrake                          : 1; // bit 0
    uint8_t seatbelt_warning                   : 1; // bit 1  lights the instrument cluster also
    uint8_t passenger_airbag_deactivated       : 1; // bit 2
    uint8_t screen_washer_liquid_level_too_low : 1; // bit 3
    uint8_t current_speed_too_high             : 1; // bit 4
    uint8_t ignition_key_left_in               : 1; // bit 5
    uint8_t sidelights_left_on                 : 1; // bit 6
    uint8_t hill_holder_active                 : 1; // bit 7  on RT3 monochrome: Driver's seatbelt not fastened
} VanDisplayMsgV2Byte5Struct;

typedef struct {
    uint8_t shock_sensor_faulty                  : 1; // bit 0
    uint8_t seatbelt_warning_                    : 1; // bit 1 not sure if it is seatbelt warning
    uint8_t check_and_reinitialize_tyre_pressure : 1; // bit 2
    uint8_t remote_control_battery_low           : 1; // bit 3
    uint8_t left_stick_button                    : 1; // bit 4
    uint8_t put_automatic_gearbox_in_P_position  : 1; // bit 5
    uint8_t stop_lights_test_brake_gently        : 1; // bit 6
    uint8_t fuel_level_low                       : 1; // bit 7  lights the instrument cluster also
} VanDisplayMsgV2Byte6Struct;

typedef struct {
    uint8_t automatic_headlamp_lighting_deactivated : 1; // bit 0
    uint8_t rear_lh_passenger_seatbelt_not_fastened : 1; // bit 1
    uint8_t rear_rh_passenger_seatbelt_not_fastened : 1; // bit 2
    uint8_t front_passenger_seatbelt_not_fastened   : 1; // bit 3
    uint8_t driving_school_pedals_indication        : 1; // bit 4
    uint8_t number_of_tpms_missing                  : 3; // bit 5-7  RT3 displays: X tyre pressure sensors missing where x equals this number
} VanDisplayMsgV2Byte7Struct;

typedef struct {
    uint8_t doors_locked                 : 1; // bit 0
    uint8_t esp_asr_deactivated          : 1; // bit 1
    uint8_t child_safety_activated       : 1; // bit 2
    uint8_t deadlocking_active           : 1; // bit 3
    uint8_t automatic_lighting_active    : 1; // bit 4
    uint8_t automatic_wiping_active      : 1; // bit 5
    uint8_t engine_immobilizer_fault     : 1; // bit 6
    uint8_t sport_suspension_mode_active : 1; // bit 7
} VanDisplayMsgV2Byte8Struct;

typedef struct {
    uint8_t                                 : 1; // bit 0
    uint8_t                                 : 1; // bit 1
    uint8_t                                 : 1; // bit 2
    uint8_t change_of_fuel_used_in_progress : 1; // bit 3
    uint8_t lpg_fuel_refused                : 1; // bit 4
    uint8_t lpg_system_faulty               : 1; // bit 5
    uint8_t lpg_in_use                      : 1; // bit 6
    uint8_t min_level_lpg                   : 1; // bit 7
} VanDisplayMsgV2Byte10Struct;

typedef struct {
    uint8_t adin_fault                    : 1; // bit 0
    uint8_t use_stop_start                : 1; // bit 1
    uint8_t stop_start_available          : 1; // bit 2
    uint8_t stop_start_activated          : 1; // bit 3
    uint8_t stop_start_deactivated        : 1; // bit 4
    uint8_t stop_start_deferred           : 1; // bit 5
    uint8_t unknown6                      : 1; // bit 6 displays empty messagebox on RT3 monochrome when the Message byte is 0x5E
    uint8_t unknown7                      : 1; // bit 7 displays empty messagebox on RT3 monochrome when the Message byte is 0x5F
} VanDisplayMsgV2Byte11Struct;

typedef struct {
    uint8_t                               : 1; // bit 0
    uint8_t                               : 1; // bit 1
    uint8_t                               : 1; // bit 2
    uint8_t                               : 1; // bit 3
    uint8_t                               : 1; // bit 4
    uint8_t                               : 1; // bit 5
    uint8_t                               : 1; // bit 6
    uint8_t change_to_neutral             : 1; // bit 7
} VanDisplayMsgV2Byte12Struct;


//Read left to right in documentation
struct psa_van_car_status_2_long {
    VanDisplayMsgV2Byte0Struct Field0;
    VanDisplayMsgV2Byte1Struct Field1;
    VanDisplayMsgV2Byte2Struct Field2;
    VanDisplayMsgV2Byte3Struct Field3;
    VanDisplayMsgV2Byte4Struct Field4;
    VanDisplayMsgV2Byte5Struct Field5;
    VanDisplayMsgV2Byte6Struct Field6;
    VanDisplayMsgV2Byte7Struct Field7;
    VanDisplayMsgV2Byte8Struct Field8;
    uint8_t Message;
    VanDisplayMsgV2Byte10Struct Field10;
    VanDisplayMsgV2Byte11Struct Field11;
    VanDisplayMsgV2Byte12Struct Field12;
    uint8_t Field13;
    uint8_t Field14;
    uint8_t Field15;
} __attribute__((packed));

struct psa_van_car_status_2_short {
    VanDisplayMsgV2Byte0Struct Field0;
    VanDisplayMsgV2Byte1Struct Field1;
    VanDisplayMsgV2Byte2Struct Field2;
    VanDisplayMsgV2Byte3Struct Field3;
    VanDisplayMsgV2Byte4Struct Field4;
    VanDisplayMsgV2Byte5Struct Field5;
    VanDisplayMsgV2Byte6Struct Field6;
    VanDisplayMsgV2Byte7Struct Field7;
    VanDisplayMsgV2Byte8Struct Field8;
    uint8_t Message;
    VanDisplayMsgV2Byte10Struct Field10;
    VanDisplayMsgV2Byte11Struct Field11;
    VanDisplayMsgV2Byte12Struct Field12;
    uint8_t Field13;
} __attribute__((packed));

#endif // PSA_VAN_CAR_STATUS_STRUCT_H