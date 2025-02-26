/****************************************************************************
 *          sdata.js
 *
 *          SData - Structured Data
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/

/***************************************************************
 *              Data Types (Enum)
 ***************************************************************/
const data_type_t = Object.freeze({
    DTP_STRING:  1,
    DTP_BOOLEAN: 2,
    DTP_INTEGER: 3,
    DTP_REAL:    4,
    DTP_LIST:    5,
    DTP_DICT:    6,
    DTP_JSON:    7,
    DTP_POINTER: 8
});

// Macros as functions
const DTP_IS_STRING  = (type) => type === data_type_t.DTP_STRING;
const DTP_IS_BOOLEAN = (type) => type === data_type_t.DTP_BOOLEAN;
const DTP_IS_INTEGER = (type) => type === data_type_t.DTP_INTEGER;
const DTP_IS_REAL    = (type) => type === data_type_t.DTP_REAL;
const DTP_IS_LIST    = (type) => type === data_type_t.DTP_LIST;
const DTP_IS_DICT    = (type) => type === data_type_t.DTP_DICT;
const DTP_IS_JSON    = (type) => type === data_type_t.DTP_JSON;
const DTP_IS_POINTER = (type) => type === data_type_t.DTP_POINTER;
const DTP_IS_SCHEMA  = (type) => type === DTP_SCHEMA;
const DTP_SCHEMA     = data_type_t.DTP_POINTER;
const DTP_IS_NUMBER  = (type) => DTP_IS_INTEGER(type) || DTP_IS_REAL(type) || DTP_IS_BOOLEAN(type);

/***************************************************************
 *              SData Field Flags
 ***************************************************************/
const sdata_flag_t = Object.freeze({
    SDF_NOTACCESS:  0x00000001,
    SDF_RD:         0x00000002,
    SDF_WR:         0x00000004,
    SDF_REQUIRED:   0x00000008,
    SDF_PERSIST:    0x00000010,
    SDF_VOLATIL:    0x00000020,
    SDF_RESOURCE:   0x00000040,
    SDF_PKEY:       0x00000080,
    SDF_FUTURE1:    0x00000100,
    SDF_FUTURE2:    0x00000200,
    SDF_WILD_CMD:   0x00000400,
    SDF_STATS:      0x00000800,
    SDF_FKEY:       0x00001000,
    SDF_RSTATS:     0x00002000,
    SDF_PSTATS:     0x00004000,
    SDF_AUTHZ_R:    0x00008000,
    SDF_AUTHZ_W:    0x00010000,
    SDF_AUTHZ_X:    0x00020000,
    SDF_AUTHZ_P:    0x00040000,
    SDF_AUTHZ_S:    0x00080000,
    SDF_AUTHZ_RS:   0x00100000
});

const SDF_PUBLIC_ATTR = sdata_flag_t.SDF_RD | sdata_flag_t.SDF_WR | sdata_flag_t.SDF_STATS |
                        sdata_flag_t.SDF_PERSIST | sdata_flag_t.SDF_VOLATIL |
                        sdata_flag_t.SDF_RSTATS | sdata_flag_t.SDF_PSTATS;
const ATTR_WRITABLE   = sdata_flag_t.SDF_WR | sdata_flag_t.SDF_PERSIST;
const ATTR_READABLE   = SDF_PUBLIC_ATTR;

/***************************************************************
 *              SData Description Structure
 ***************************************************************/
class SDataDesc {
    constructor(type, name, flag = 0, default_value = null, description = "",
                alias = null, header = null, fillspace = 0, json_fn = null, schema = null, authpth = null) {
        this.type         = type;
        this.name         = name;
        this.alias        = alias;
        this.flag         = flag;
        this.default_value= default_value;
        this.header       = header;
        this.fillspace    = fillspace;
        this.description  = description;
        this.json_fn      = json_fn;
        this.schema       = schema;
        this.authpth      = authpth;
    }
}

/***************************************************************
 *              SDATA Macros as Functions
 ***************************************************************/
const SDATA_END   = () => new SDataDesc(0, null);
const SDATA       = (type, name, flag, default_value, description) => new SDataDesc(type, name, flag, default_value, description);
const SDATACM     = (type, name, alias, items, json_fn, description) => new SDataDesc(type, name, 0, null, description, alias, null, 0, json_fn, items, null);
const SDATACM2    = (type, name, flag, alias, items, json_fn, description) => new SDataDesc(type, name, flag, null, description, alias, null, 0, json_fn, items, null);
const SDATAPM     = (type, name, flag, default_value, description) => new SDataDesc(type, name, flag, default_value, description);
const SDATAAUTHZ  = (type, name, flag, alias, items, description) => new SDataDesc(type, name, flag, null, description, alias, null, 0, null, items, null);
const SDATAPM0    = (type, name, flag, authpth, description) => new SDataDesc(type, name, flag, null, description, null, null, 0, null, null, authpth);
const SDATADF     = (type, name, flag, header, fillspace, description) => new SDataDesc(type, name, flag, null, description, null, header, fillspace);

export { data_type_t, sdata_flag_t, SDataDesc, SDATA, SDATACM, SDATACM2, SDATAPM, SDATAAUTHZ, SDATAPM0, SDATADF, SDATA_END };
