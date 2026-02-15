// message_fields.h - Constants for JSON field names used in vent system communication
#ifndef MESSAGE_FIELDS_H
#define MESSAGE_FIELDS_H

// STATUS_UPDATE field names (CEE → REW)
#define FIELD_FAN_WC              "fwc"   // fan_wc
#define FIELD_FAN_UTILITY         "fut"   // fan_utility
#define FIELD_FAN_BATHROOM        "fkop"  // fan_bathroom
#define FIELD_FAN_LIVING_EXH      "fdse"  // fan_living_exhaust
#define FIELD_FAN_LIVING_INT      "fdsi"  // fan_living_intake // opuščeno
#define FIELD_FAN_LIVING          "fliv"  // fan_living // opuščeno

#define FIELD_INPUT_BATHROOM_BTN  "ibt"   // input_bathroom_button // opuščeno
#define FIELD_INPUT_UTILITY_SW    "iut"   // input_utility_switch // opuščeno
#define FIELD_INPUT_BATHROOM_L1   "il1"   // input_bathroom_light_1
#define FIELD_INPUT_BATHROOM_L2   "il2"   // input_bathroom_light_2
#define FIELD_INPUT_UTILITY_L     "iul"   // input_utility_light
#define FIELD_INPUT_WC_L          "iwc"   // input_wc_light
#define FIELD_INPUT_WINDOW_ROOF   "iwr"   // input_window_roof
#define FIELD_INPUT_WINDOW_BALC   "iwb"   // input_window_balcony

#define FIELD_TIME_WC             "twc"   // time_wc
#define FIELD_TIME_UTILITY        "tut"   // time_utility
#define FIELD_TIME_BATHROOM       "tkop"  // time_bathroom
#define FIELD_TIME_LIVING_EXH     "tdse"  // time_living_exhaust
#define FIELD_TIME_LIVING_INT     "tdsi"  // time_living_intake // opuščeno
#define FIELD_TIME_LIVING         "tliv"  // time_living // opuščeno

#define FIELD_ERROR_BME280        "ebm"   // error_bme280
#define FIELD_ERROR_SHT41         "esht"  // error_sht41
#define FIELD_ERROR_POWER         "epwr"  // error_power
#define FIELD_ERROR_DEW           "edew"  // error_dew
#define FIELD_ERROR_TIME_SYNC     "etms"  // error_time_sync

#define FIELD_TEMP_BATHROOM       "tbat"  // temperature_bathroom
#define FIELD_HUM_BATHROOM        "hbat"  // humidity_bathroom
#define FIELD_PRESS_BATHROOM      "pbat"  // pressure_bathroom
#define FIELD_TEMP_UTILITY        "tutl"  // temperature_utility
#define FIELD_HUM_UTILITY         "hutl"  // humidity_utility

#define FIELD_CURRENT_POWER       "pwr"   // current_power
#define FIELD_ENERGY_CONSUMPTION  "eng"   // energy_consumption
#define FIELD_DUTY_CYCLE_LIVING   "dlds"  // duty_cycle_living_room

// DEW_UPDATE field names (CEE → DEW)
#define FIELD_DEW_FAN             "df"    // dew_fan
#define FIELD_DEW_OFF_TIME        "do"    // dew_off_timestamp
#define FIELD_DEW_TEMP            "dt"    // dew_temperature
#define FIELD_DEW_HUM             "dh"    // dew_humidity
#define FIELD_DEW_ERROR           "de"    // dew_error
#define FIELD_WEATHER_ICON        "wi"    // weather_icon
#define FIELD_SEASON_CODE         "ss"    // season_code

// SENSOR_DATA field names (REW → CEE)
#define FIELD_EXT_TEMP            "et"    // external_temperature
#define FIELD_EXT_HUM             "eh"    // external_humidity
#define FIELD_EXT_PRESS           "ep"    // external_pressure
#define FIELD_DS_TEMP             "dt"    // ds_temperature
#define FIELD_DS_HUM              "dh"    // ds_humidity
#define FIELD_DS_CO2              "dc"    // ds_co2
#define FIELD_WEATHER_ICON        "wi"    // weather_icon
#define FIELD_SEASON_CODE         "ss"    // season_code
#define FIELD_TIMESTAMP           "ts"    // timestamp

// MANUAL_CONTROL field names (REW → CEE)
#define FIELD_ROOM                "room"  // room
#define FIELD_ACTION              "action"// action

// LOGS field names (CEE → REW)
#define FIELD_LOGS                "logs"  // logs content

#endif // MESSAGE_FIELDS_H