/***********************************************************************
 *          ui_lib_devices.js
 *
 *          Devices Helpers
 *
 *          Copyright (c) 2020-2024 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    log_error,
    sprintf,
} from "yunetas";

/************************************************************
 *
 ************************************************************/
function device_measures(device_type, get_i18n, get_units)
{
    let fields = null;
    let i18n = null;
    let units = null;

    switch (device_type) {
        case "C_GATE_ESTERILIZ-gate_esteriliz":
            // {"ST",                  "str",  ""},
            //
            // {"serial_number",       "str",  ""},
            // {"time",                "str",  ""},
            // {"program_number",      "int",  "0"},
            // {"program_name",        "str",  ""},
            // {"cycle",               "int",  "0"},
            // {"alarm",               "int",  "0"},
            // {"phase",               "int",  "0"},
            // {"temperature",         "real", "0"},
            // {"Ao",                  "int",  "0"},
            // {"inflow_name",         "str",  ""},
            // {"inflow_value",        "real", "0"},
            // {"inflow_metric",       "str",  ""},
            // {"ta",                  "int",  "0"},
            // {"cause",               "str",  ""},
            fields = [
                "ST"
                // "serial_number",
                // "time",
                // "program_number",
                // "program_name",
                // "cycle",
                // "alarm",
                // "phase",
                // "temperature",
                // "Ao",
                // "inflow_name",
                // "inflow_value",
                // "inflow_metric",
                // "ta",
                // "cause"
            ];
            break;

        case "C_GATE_AURAAIR-gate_auraair":
        case "C_GATE_MQTT-gate_mqtt":
        case "C_GATE_MQTT-FS00802_WIFI":
            // {"temperature",         "real", "0"},
            // {"humidity",            "real", "0"},
            // {"co2",                 "int",  "0"},
            // {"voc",                 "int",  "0"},
            // {"pm2",                 "int",  "0"},
            // {"pm10",                "int",  "0"},
            // {"dominant_pollutant",  "str",  ""},
            // {"aqi",                 "int",  "0"},
            // {"co",                  "real", "0"},
            // {"noise",               "int",  "0"},
            // {"battery_level",       "real", "0"},
            fields = [
                "temperature",
                "humidity",
                "co2",
                "pm2",
                "pm10",
                "pm1_0",
                "pm25",
                "tvoc",
                "hcho",     // nuevo, medida (ppm), Formaldehído
                "c8h10",    // nuevo, medida (µg/m3), Xileno
                "noise",
                "battery_level",
            ];
            break;

        case "C_GATE_SONDA-gate_sonda":
            // {"temperature",         "real", "0"},
            // {"voltage",             "real", "0"},
            // {"pt",                  "real", "0"},
            fields = [
                "temperature",
                "voltage",
                "pt"
            ];
            break;

        case "C_GATE_ENCHUFE-gate_enchufe":
            // {"power_on",            "str",  "OFF"},
            // {"voltage",             "real", "0"},
            // {"temperature",         "real", "0"},
            // {"total",               "real", "0"},
            // {"yesterday",           "real", "0"},
            // {"today",               "real", "0"},
            // {"period",              "real", "0"},
            // {"power",               "real", "0"},
            // {"apparentpower",       "real", "0"},
            // {"reactivepower",       "real", "0"},
            // {"factor",              "real", "0"},
            // {"current",             "real", "0"},
            fields = [ // WARNING fields, units and i18m must be in same order
                "power_on",
                "total",
                "yesterday",
                "today",
                "power",
                "apparentPower",
                "reactivePower",
                "current",
                "voltage",
                "temperature",
                "factor",
                // "period",
            ];
            i18n = [ // WARNING fields, units and i18m must be in same order
                "enchufe.power_on",
                "enchufe.total",
                "enchufe.yesterday",
                "enchufe.today",
                "enchufe.power",
                "enchufe.apparentPower",
                "enchufe.reactivePower",
                "enchufe.current",
                "enchufe.voltage",
                "enchufe.temperature",
                "enchufe.factor",
                // "enchufe.period",
            ];
            units = [ // WARNING fields, units and i18m must be in same order
                "",
                "kWh",
                "kWh",
                "kWh",
                "W",
                "VA",
                "VAr",
                "A",
                "V",
                "º",
                "",
                //"",
            ];

            break;

        case "C_GATE_FRIGO-gate_frigo":
            // {"ST",                  "int",  "0"},
            // {"AL",                  "int",  "0"},
            // {"DOUT",                "int",  "0"},
            // {"DIN",                 "int",  "0"},
            // {"T1",                  "real", "0"},
            // {"T2",                  "real", "0"},
            // {"T3",                  "real", "0"},
            // {"TS",                  "int",  "0"},
            // {"S1",                  "int",  "0"},
            // {"S2",                  "int",  "0"},
            // {"S3",                  "int",  "0"},
            // {"AA",                  "int",  "0"},
            // {"CO1",                 "int",  "0"},
            // {"CO2",                 "int",  "0"},
            // {"MD_MA",               "str",  ""},
            // {"MD_MO",               "str",  ""},
            // {"AL00",                "int",  "0"},

            fields = [
                "temperature",
                "ST",
                "AL",
                "DOUT",
                "DIN",
                "T1",
                "T2",
                "T3",
                "TS",
                "S1",
                "S2",
                "S3",
                "AA",
                "CO1",
                "CO2"
            ];
            break;

        case "C_GATE_CAUDAL-gate_caudal":
            fields = [
                "FlowRate",
                "EnergyFlowRate",
                "Velocity",
                "SoundSpeed",
                "Net_Accumulator",
            ];
            break;

        default:
            log_error(sprintf("unknown device type %s", device_type));
    }
    if(get_i18n) {
        if(i18n) {
            return i18n;
        } else {
            return fields;
        }
    }
    if(get_units) {
        return units;
    }
    return fields;
}

export {
    device_measures
};
