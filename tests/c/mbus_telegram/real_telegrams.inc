/*
 *  Telegrams of real meters, from test/test-frames of libmbus (BSD 3-Clause
 *  License, see LICENSE-libmbus-frames.txt), with what this decoder answers
 *  for them, checked against libmbus (see test_mbus_telegram.c).
 *  {title, hex, more records follow, expected json}
 */
    {
        "Kamstrup Multical 601, heat: tariffs, subunits, max values, a date",
        "68 F7 F7 68 08 11 72 17 58 85 06 2D 2C 08 04 04 00 00 00 0C 78 17 58 85 "
        "06 04 06 E7 91 00 00 04 14 2C DB 00 00 04 22 D9 03 00 00 04 59 B9 27 00 "
        "00 04 5D 08 12 00 00 04 61 B1 15 00 00 04 2D 5B 01 00 00 14 2D C0 01 00 "
        "00 04 3B 1F 02 00 00 14 3B 74 02 00 00 84 10 06 00 00 00 00 84 20 06 00 "
        "00 00 00 84 40 14 00 00 00 00 84 80 40 14 00 00 00 00 84 C0 40 06 00 00 "
        "00 00 04 6D 1A 2F 65 11 44 06 51 82 00 00 44 14 B2 C3 00 00 54 2D 26 02 "
        "00 00 54 3B 03 04 00 00 C4 10 06 00 00 00 00 C4 20 06 00 00 00 00 C4 40 "
        "14 00 00 00 00 C4 80 40 14 00 00 00 00 C4 C0 40 06 00 00 00 00 42 6C 5F "
        "1C 0F 00 00 00 00 E7 E4 00 00 63 66 00 00 00 00 00 00 00 00 00 00 00 00 "
        "00 00 5B C9 A5 02 34 53 00 00 E0 B2 03 00 89 9C 68 00 00 00 00 00 01 00 "
        "01 07 07 09 01 03 00 00 00 00 00 98 16",
        FALSE,
        "{\"header\":{\"id\":\"06855817\",\"manufacturer\":\"KAM\",\"version\":8,\"med"
        "ium\":\"heat\",\"medium_code\":4,\"access_no\":4,\"status\":0},\"records\":"
        "{\"fabrication_number\":{\"value\":\"06855817\"},\"energy\":{\"value\":373"
        "51000,\"unit\":\"Wh\"},\"volume\":{\"value\":561.08,\"unit\":\"m3\"},\"on_tim"
        "e\":{\"value\":3546000,\"unit\":\"s\"},\"flow_temperature\":{\"value\":101."
        "69,\"unit\":\"C\"},\"return_temperature\":{\"value\":46.16,\"unit\":\"C\"},\""
        "temperature_difference\":{\"value\":55.53,\"unit\":\"K\"},\"power\":{\"val"
        "ue\":34700,\"unit\":\"W\"},\"power:max\":{\"value\":44800,\"unit\":\"W\"},\"vo"
        "lume_flow\":{\"value\":0.543,\"unit\":\"m3/h\"},\"volume_flow:max\":{\"val"
        "ue\":0.628,\"unit\":\"m3/h\"},\"energy@t1\":{\"value\":0,\"unit\":\"Wh\"},\"en"
        "ergy@t2\":{\"value\":0,\"unit\":\"Wh\"},\"volume@u1\":{\"value\":0.0,\"unit\""
        ":\"m3\"},\"volume@u2\":{\"value\":0.0,\"unit\":\"m3\"},\"energy@u3\":{\"value"
        "\":0,\"unit\":\"Wh\"},\"datetime\":{\"value\":\"2011-01-05T15:26\"},\"energy"
        "@s1\":{\"value\":33361000,\"unit\":\"Wh\"},\"volume@s1\":{\"value\":500.98,"
        "\"unit\":\"m3\"},\"power:max@s1\":{\"value\":55000,\"unit\":\"W\"},\"volume_f"
        "low:max@s1\":{\"value\":1.027,\"unit\":\"m3/h\"},\"energy@s1@t1\":{\"value"
        "\":0,\"unit\":\"Wh\"},\"energy@s1@t2\":{\"value\":0,\"unit\":\"Wh\"},\"volume@"
        "s1@u1\":{\"value\":0.0,\"unit\":\"m3\"},\"volume@s1@u2\":{\"value\":0.0,\"un"
        "it\":\"m3\"},\"energy@s1@u3\":{\"value\":0,\"unit\":\"Wh\"},\"date@s1\":{\"val"
        "ue\":\"2010-12-31\"}},\"mfr_data\":\"00000000E7E4000063660000000000000"
        "0000000000000005BC9A50234530000E0B20300899C680000000000010001070"
        "70901030000000000\"}"
    },
    {
        "Itron Cyble, water: plain text VIFs, a manufacturer VIFE",
        "68 56 56 68 08 01 72 71 00 00 12 77 04 14 07 0A 30 00 00 0C 78 71 00 00 "
        "12 0D 7C 08 44 49 20 2E 74 73 75 63 0A 45 4C 42 59 43 20 54 53 45 54 04 "
        "6D 2B 0D 98 11 02 7C 09 65 6D 69 74 20 2E 74 61 62 F2 10 04 14 3D 30 00 "
        "00 04 94 7F 14 00 00 00 44 14 00 00 00 00 0F 10 01 1F 2F 16",
        FALSE,
        "{\"header\":{\"id\":\"12000071\",\"manufacturer\":\"ACW\",\"version\":20,\"me"
        "dium\":\"water\",\"medium_code\":7,\"access_no\":10,\"status\":48},\"recor"
        "ds\":{\"fabrication_number\":{\"value\":\"12000071\"},\"custom\":{\"value\""
        ":\"TEST CYBLE\",\"unit\":\"cust. ID\"},\"datetime\":{\"value\":\"2012-01-24"
        "T13:43\"},\"custom#2\":{\"value\":4338,\"unit\":\"bat. time\"},\"volume\":{"
        "\"value\":123.49,\"unit\":\"m3\"},\"volume~7f\":{\"value\":0.2},\"volume@s1"
        "\":{\"value\":0.0,\"unit\":\"m3\"}},\"mfr_data\":\"10011F\"}"
    },
    {
        "Itron Cyble, gas",
        "68 56 56 68 08 04 72 87 03 02 10 77 04 14 03 9A 00 00 00 0C 78 87 03 02 "
        "10 0D 7C 08 44 49 20 2E 74 73 75 63 0A 20 20 20 20 20 20 20 20 20 20 04 "
        "6D 2B 0F 79 1A 02 7C 09 65 6D 69 74 20 2E 74 61 62 D2 0F 04 14 1A 00 00 "
        "00 04 94 7F 00 00 00 00 44 14 19 00 00 00 0F 00 02 1F 9F 16",
        FALSE,
        "{\"header\":{\"id\":\"10020387\",\"manufacturer\":\"ACW\",\"version\":20,\"me"
        "dium\":\"gas\",\"medium_code\":3,\"access_no\":154,\"status\":0},\"records"
        "\":{\"fabrication_number\":{\"value\":\"10020387\"},\"custom\":{\"value\":\""
        "          \",\"unit\":\"cust. ID\"},\"datetime\":{\"value\":\"2011-10-25T1"
        "5:43\"},\"custom#2\":{\"value\":4050,\"unit\":\"bat. time\"},\"volume\":{\"v"
        "alue\":0.26,\"unit\":\"m3\"},\"volume~7f\":{\"value\":0.0},\"volume@s1\":{\""
        "value\":0.25,\"unit\":\"m3\"}},\"mfr_data\":\"00021F\"}"
    },
    {
        "Engelmann WaterStar, warm water: dates, error flags, a per-pulse VIFE",
        "68 51 51 68 08 0B 72 54 02 99 04 C5 14 00 06 0C 27 00 00 04 78 2E 25 4C "
        "00 04 6D 0A 0C CD 13 04 13 4C 01 00 00 44 13 4B 01 00 00 84 01 13 4C 01 "
        "00 00 42 6C BF 1C 02 6C DF 1C 04 3B 00 00 00 00 14 3B 16 08 00 00 02 23 "
        "A7 04 01 FD 17 00 04 90 28 08 00 00 00 3F 16",
        FALSE,
        "{\"header\":{\"id\":\"04990254\",\"manufacturer\":\"EFE\",\"version\":0,\"med"
        "ium\":\"warm_water\",\"medium_code\":6,\"access_no\":12,\"status\":39},\"r"
        "ecords\":{\"fabrication_number\":{\"value\":\"4990254\"},\"datetime\":{\"v"
        "alue\":\"2014-03-13T12:10\"},\"volume\":{\"value\":0.332,\"unit\":\"m3\"},\""
        "volume@s1\":{\"value\":0.331,\"unit\":\"m3\"},\"volume@s2\":{\"value\":0.33"
        "2,\"unit\":\"m3\"},\"date@s1\":{\"value\":\"2013-12-31\"},\"date\":{\"value\":"
        "\"2014-12-31\"},\"volume_flow\":{\"value\":0.0,\"unit\":\"m3/h\"},\"volume_"
        "flow:max\":{\"value\":2.07,\"unit\":\"m3/h\"},\"on_time\":{\"value\":102902"
        "400,\"unit\":\"s\"},\"error_flags\":{\"value\":0},\"volume~28\":{\"value\":8"
        "e-06}}}"
    },
    {
        "Landis+Gyr Ultraheat T230, heat: 'date of' VIFEs (6F), storage 510",
        "68 E2 E2 68 08 00 72 05 02 66 66 A7 32 07 04 01 10 00 00 09 74 04 09 70 "
        "08 0C 06 00 00 00 00 0C 14 00 00 00 00 0B 2D 00 00 00 0B 3B 00 00 00 0B "
        "5A 95 01 00 0B 5E 97 01 00 0B 62 02 00 F0 0C 78 05 02 66 66 89 10 71 07 "
        "3C 22 69 37 00 00 0C 22 69 37 00 00 0C 26 00 00 00 00 8C 90 10 06 00 00 "
        "00 00 9B 10 2D 00 00 00 9B 10 3B 00 00 00 9B 10 5A 07 03 00 9B 10 5E 07 "
        "05 00 94 10 AD 6F 00 00 00 00 94 10 BB 6F 00 00 00 00 94 10 DA 6F 32 14 "
        "7A 18 94 10 DE 6F 2B 0B 69 18 4C 06 00 00 00 00 4C 14 00 00 00 00 7C 22 "
        "69 34 00 00 4C 26 00 00 00 00 CC 90 10 06 00 00 00 00 DB 10 2D 00 00 00 "
        "DB 10 3B 00 00 00 DB 10 5A 07 03 00 DB 10 5E 07 05 00 84 8F 0F 6D 00 00 "
        "E1 F1 04 6D 04 0C 8D 11 0F 09 07 00 66 01 7D 16",
        FALSE,
        "{\"header\":{\"id\":\"66660205\",\"manufacturer\":\"LUG\",\"version\":7,\"med"
        "ium\":\"heat\",\"medium_code\":4,\"access_no\":1,\"status\":16},\"records\""
        ":{\"actuality_duration\":{\"value\":4,\"unit\":\"s\"},\"averaging_duratio"
        "n\":{\"value\":8,\"unit\":\"s\"},\"energy\":{\"value\":0,\"unit\":\"Wh\"},\"volu"
        "me\":{\"value\":0.0,\"unit\":\"m3\"},\"power\":{\"value\":0,\"unit\":\"W\"},\"vo"
        "lume_flow\":{\"value\":0.0,\"unit\":\"m3/h\"},\"flow_temperature\":{\"valu"
        "e\":19.5,\"unit\":\"C\"},\"return_temperature\":{\"value\":19.7,\"unit\":\"C"
        "\"},\"temperature_difference\":{\"value\":-0.2,\"unit\":\"K\"},\"fabricati"
        "on_number\":{\"value\":\"66660205\"},\"averaging_duration@t1\":{\"value\""
        ":420,\"unit\":\"s\"},\"on_time:error\":{\"value\":13568400,\"unit\":\"s\"},\""
        "on_time\":{\"value\":13568400,\"unit\":\"s\"},\"operating_time\":{\"value\""
        ":0,\"unit\":\"s\"},\"energy@t5\":{\"value\":0,\"unit\":\"Wh\"},\"power:max@t1"
        "\":{\"value\":0,\"unit\":\"W\"},\"volume_flow:max@t1\":{\"value\":0.0,\"unit"
        "\":\"m3/h\"},\"flow_temperature:max@t1\":{\"value\":30.7,\"unit\":\"C\"},\"r"
        "eturn_temperature:max@t1\":{\"value\":50.7,\"unit\":\"C\"},\"power:max@t"
        "1~6f\":{\"value\":null},\"volume_flow:max@t1~6f\":{\"value\":null},\"flo"
        "w_temperature:max@t1~6f\":{\"value\":\"2011-08-26T20:50\"},\"return_te"
        "mperature:max@t1~6f\":{\"value\":\"2011-08-09T11:43\"},\"energy@s1\":{\""
        "value\":0,\"unit\":\"Wh\"},\"volume@s1\":{\"value\":0.0,\"unit\":\"m3\"},\"on_"
        "time:error@s1\":{\"value\":12488400,\"unit\":\"s\"},\"operating_time@s1\""
        ":{\"value\":0,\"unit\":\"s\"},\"energy@s1@t5\":{\"value\":0,\"unit\":\"Wh\"},\""
        "power:max@s1@t1\":{\"value\":0,\"unit\":\"W\"},\"volume_flow:max@s1@t1\":"
        "{\"value\":0.0,\"unit\":\"m3/h\"},\"flow_temperature:max@s1@t1\":{\"value"
        "\":30.7,\"unit\":\"C\"},\"return_temperature:max@s1@t1\":{\"value\":50.7,"
        "\"unit\":\"C\"},\"datetime@s510\":{\"value\":null},\"dateti"
        "me\":{\"value\":\"2012-01-13T12:04\"}},\"mfr_data\":\"0907006601\"}"
    },
    {
        "Sontex Supercal 531, heat: more records follow (DIF 1F)",
        "68 51 51 68 08 01 72 24 06 42 08 EE 4D 0D 04 2C 30 00 00 04 0E 00 00 00 "
        "00 04 14 00 00 00 00 05 5B 00 00 00 00 05 5F 00 00 00 00 05 3E 00 00 00 "
        "00 05 2B 00 00 00 00 C4 00 0E 00 00 00 00 C4 00 14 00 00 00 00 C4 40 14 "
        "00 00 00 00 C4 80 40 14 00 00 00 00 1F 71 16",
        TRUE,
        "{\"header\":{\"id\":\"08420624\",\"manufacturer\":\"SON\",\"version\":13,\"me"
        "dium\":\"heat\",\"medium_code\":4,\"access_no\":44,\"status\":48},\"record"
        "s\":{\"energy\":{\"value\":0.0,\"unit\":\"Wh\"},\"volume\":{\"value\":0.0,\"un"
        "it\":\"m3\"},\"flow_temperature\":{\"value\":0.0,\"unit\":\"C\"},\"return_te"
        "mperature\":{\"value\":0.0,\"unit\":\"C\"},\"volume_flow\":{\"value\":0.0,\""
        "unit\":\"m3/h\"},\"power\":{\"value\":0.0,\"unit\":\"W\"},\"energy@s1\":{\"val"
        "ue\":0.0,\"unit\":\"Wh\"},\"volume@s1\":{\"value\":0.0,\"unit\":\"m3\"},\"volu"
        "me@s1@u1\":{\"value\":0.0,\"unit\":\"m3\"},\"volume@s1@u2\":{\"value\":0.0,"
        "\"unit\":\"m3\"}}}"
    },
